// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <set>

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"

#include "mamba/core/channel.hpp"
#include "mamba/core/link.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/link.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/virtual_packages.hpp"

#include "termcolor/termcolor.hpp"

namespace mamba
{
    static std::map<std::string, std::string> other_pkg_mgr_install_instructions
        = { { "pip", "pip install -r {0} --no-input" } };

    auto install_for_other_pkgmgr(const detail::other_pkg_mgr_spec& other_spec)
    {
        const auto& pkg_mgr = other_spec.pkg_mgr;
        const auto& deps = other_spec.deps;
        const auto& cwd = other_spec.cwd;

        std::string install_instructions = other_pkg_mgr_install_instructions[pkg_mgr];

        TemporaryFile specs;
        std::ofstream specs_f(specs.path());
        for (auto& d : deps)
            specs_f << d.c_str() << '\n';
        specs_f.close();

        replace_all(install_instructions, "{0}", specs.path());

        const auto& ctx = Context::instance();

        std::vector<std::string> install_args = split(install_instructions, " ");

        install_args[0]
            = (ctx.target_prefix / get_bin_directory_short_path() / install_args[0]).string();
        auto [wrapped_command, tmpfile] = prepare_wrapped_call(ctx.target_prefix, install_args);

        reproc::options options;
        options.redirect.parent = true;
        options.working_directory = cwd.c_str();

        Console::stream() << "\n"
                          << termcolor::cyan << "Installing " << pkg_mgr
                          << " packages: " << join(", ", deps) << termcolor::reset;
        LOG_INFO << "Calling: " << join(" ", install_args);

        auto [status, ec] = reproc::run(wrapped_command, options);
        assert_reproc_success(options, status, ec);
    }

    auto& truthy_values()
    {
        static std::map<std::string, int> vals{
            { "win", 0 },
            { "unix", 0 },
            { "osx", 0 },
            { "linux", 0 },
        };

        const auto& ctx = Context::instance();
        if (starts_with(ctx.platform, "win"))
        {
            vals["win"] = true;
        }
        else
        {
            vals["unix"] = true;
            if (starts_with(ctx.platform, "linux"))
            {
                vals["linux"] = true;
            }
            else if (starts_with(ctx.platform, "osx"))
            {
                vals["osx"] = true;
            }
        }
        return vals;
    }

    namespace detail
    {
        bool eval_selector(const std::string& selector)
        {
            if (!(starts_with(selector, "sel(") && selector[selector.size() - 1] == ')'))
            {
                throw std::runtime_error(
                    "Couldn't parse selector. Needs to start with sel( and end with )");
            }
            std::string expr = selector.substr(4, selector.size() - 5);

            if (truthy_values().find(expr) == truthy_values().end())
            {
                throw std::runtime_error("Couldn't parse selector. Value not in [unix, linux, "
                                         "osx, win] or additional whitespaces found.");
            }

            return truthy_values()[expr];
        }

        yaml_file_contents read_yaml_file(fs::path yaml_file)
        {
            yaml_file_contents result;
            YAML::Node f;
            try
            {
                f = YAML::LoadFile(yaml_file);
            }
            catch (YAML::Exception& e)
            {
                LOG_ERROR << "Error in spec file: " << yaml_file;
            }

            YAML::Node deps = f["dependencies"];
            YAML::Node final_deps;

            bool has_pip_deps = false;
            for (auto it = deps.begin(); it != deps.end(); ++it)
            {
                if (it->IsScalar())
                {
                    final_deps.push_back(*it);
                }
                else if (it->IsMap())
                {
                    // we merge a map to the upper level if the selector works
                    for (const auto& map_el : *it)
                    {
                        std::string key = map_el.first.as<std::string>();
                        if (starts_with(key, "sel("))
                        {
                            bool selected = detail::eval_selector(key);
                            if (selected)
                            {
                                const YAML::Node& rest = map_el.second;
                                if (rest.IsScalar())
                                {
                                    final_deps.push_back(rest);
                                }
                                else
                                {
                                    throw std::runtime_error(
                                        "Complicated selection merge not implemented yet.");
                                }
                            }
                        }
                        else if (key == "pip")
                        {
                            result.others_pkg_mgrs_specs.push_back(
                                { "pip",
                                  map_el.second.as<std::vector<std::string>>(),
                                  fs::absolute(yaml_file.parent_path()) });
                            has_pip_deps = true;
                        }
                    }
                }
            }

            std::vector<std::string> dependencies = final_deps.as<std::vector<std::string>>();

            if (has_pip_deps && !std::count(dependencies.begin(), dependencies.end(), "pip"))
                dependencies.push_back("pip");

            result.dependencies = dependencies;

            if (f["channels"])
            {
                try
                {
                    result.channels = f["channels"].as<std::vector<std::string>>();
                }
                catch (YAML::Exception& e)
                {
                    throw std::runtime_error(mamba::concat(
                        "Could not read 'channels' as list of strings from ", yaml_file.string()));
                }
            }
            else
            {
                LOG_DEBUG << "No 'channels' specified in file: " << yaml_file;
            }

            if (f["name"])
            {
                result.name = f["name"].as<std::string>();
            }
            {
                LOG_DEBUG << "No env 'name' specified in file: " << yaml_file;
            }
            return result;
        }

        std::tuple<std::vector<PackageInfo>, std::vector<MatchSpec>> parse_urls_to_package_info(
            const std::vector<std::string>& urls)
        {
            std::vector<PackageInfo> pi_result;
            std::vector<MatchSpec> ms_result;
            for (auto& u : urls)
            {
                if (strip(u).size() == 0)
                    continue;
                std::size_t hash = u.find_first_of('#');
                MatchSpec ms(u.substr(0, hash));
                PackageInfo p(ms.name);
                p.url = ms.url;
                p.build_string = ms.build;
                p.version = ms.version;
                p.channel = ms.channel;
                p.fn = ms.fn;

                if (hash != std::string::npos)
                {
                    p.md5 = u.substr(hash + 1);
                    ms.brackets["md5"] = u.substr(hash + 1);
                }
                pi_result.push_back(p);
                ms_result.push_back(ms);
            }
            return std::make_tuple(pi_result, ms_result);
        }

        bool operator==(const other_pkg_mgr_spec& s1, const other_pkg_mgr_spec& s2)
        {
            return (s1.pkg_mgr == s2.pkg_mgr) && (s1.deps == s2.deps) && (s1.cwd == s2.cwd);
        }
    }

    void install()
    {
        auto& config = Configuration::instance();

        config.at("create_base").set_value(true);
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                       | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX);
        config.load();

        auto& install_specs = config.at("specs").value<std::vector<std::string>>();
        auto& use_explicit = config.at("explicit_install").value<bool>();

        if (!install_specs.empty())
        {
            if (use_explicit)
            {
                install_explicit_specs(install_specs, false);
            }
            else
            {
                mamba::install_specs(install_specs, false);
            }
        }
        else
        {
            Console::print("Nothing to do.");
        }

        config.operation_teardown();
    }

    int RETRY_SUBDIR_FETCH = 1 << 0;
    int RETRY_SOLVE_ERROR = 1 << 1;

    void install_specs(const std::vector<std::string>& specs,
                       bool create_env,
                       int solver_flag,
                       int is_retry)
    {
        auto& ctx = Context::instance();
        auto& config = Configuration::instance();

        auto& no_pin = config.at("no_pin").value<bool>();
        auto& no_py_pin = config.at("no_py_pin").value<bool>();
        auto& retry_clean_cache = config.at("retry_clean_cache").value<bool>();

        if (ctx.target_prefix.empty())
        {
            throw std::runtime_error("No active target prefix");
        }
        if (!fs::exists(ctx.target_prefix) && create_env == false)
        {
            LOG_ERROR << "Prefix does not exist at: " << ctx.target_prefix;
            exit(1);
        }

        MultiPackageCache package_caches(ctx.pkgs_dirs);

        // add channels from specs
        std::vector<mamba::MatchSpec> match_specs(specs.begin(), specs.end());
        for (const auto& m : match_specs)
        {
            if (!m.channel.empty())
            {
                ctx.channels.push_back(m.channel);
            }
        }

        if (ctx.channels.empty() && !ctx.offline)
        {
            LOG_WARNING << "No 'channels' specified";
        }

        std::vector<std::string> channel_urls = ctx.channels;
        // Always append context channels
        auto& ctx_channels = Context::instance().channels;
        std::copy(ctx_channels.begin(), ctx_channels.end(), std::back_inserter(channel_urls));

        std::vector<std::shared_ptr<MSubdirData>> subdirs;
        MultiDownloadTarget multi_dl;

        std::vector<std::pair<int, int>> priorities;
        int max_prio = static_cast<int>(channel_urls.size());
        std::string prev_channel_name;

        if (config.at("experimental").value<bool>())
        {
            load_tokens();
        }

        for (auto channel : get_channels(channel_urls))
        {
            for (auto& [platform, url] : channel->platform_urls(true))
            {
                std::string repodata_full_url = concat(url, "/repodata.json");

                auto sdir = std::make_shared<MSubdirData>(
                    concat(channel->canonical_name(), "/", platform),
                    repodata_full_url,
                    cache_fn_url(repodata_full_url),
                    package_caches,
                    platform == "noarch");

                sdir->load();
                multi_dl.add(sdir->target());
                subdirs.push_back(sdir);
                if (ctx.channel_priority == ChannelPriority::kDisabled)
                {
                    priorities.push_back(std::make_pair(0, max_prio--));
                }
                else  // Consider 'flexible' and 'strict' the same way
                {
                    if (channel->name() != prev_channel_name)
                    {
                        max_prio--;
                        prev_channel_name = channel->name();
                    }
                    priorities.push_back(std::make_pair(max_prio, platform == "noarch" ? 0 : 1));
                }
            }
        }
        if (!ctx.offline)
        {
            multi_dl.download(true);
        }

        std::vector<MRepo> repos;
        MPool pool;
        if (ctx.offline)
        {
            LOG_INFO << "Creating repo from pkgs_dir for offline";
            for (const auto& c : ctx.pkgs_dirs)
                repos.push_back(detail::create_repo_from_pkgs_dir(pool, c));
        }
        PrefixData prefix_data(ctx.target_prefix);
        prefix_data.load();

        std::vector<std::string> prefix_pkgs;
        for (auto& it : prefix_data.m_package_records)
            prefix_pkgs.push_back(it.first);

        prefix_data.add_virtual_packages(get_virtual_packages());

        auto repo = MRepo(pool, prefix_data);
        repos.push_back(repo);

        std::string prev_channel;
        bool loading_failed = false;
        for (std::size_t i = 0; i < subdirs.size(); ++i)
        {
            auto& subdir = subdirs[i];
            if (!subdir->loaded())
            {
                if (ctx.offline || !mamba::ends_with(subdir->name(), "/noarch"))
                {
                    continue;
                }
                else
                {
                    throw std::runtime_error("Subdir " + subdir->name() + " not loaded!");
                }
            }

            auto& prio = priorities[i];
            try
            {
                MRepo repo = subdir->create_repo(pool);
                repo.set_priority(prio.first, prio.second);
                repos.push_back(repo);
            }
            catch (std::runtime_error& e)
            {
                if (is_retry & RETRY_SUBDIR_FETCH)
                {
                    std::stringstream ss;
                    ss << "Could not load repodata.json for " << subdir->name() << " after retry."
                       << "Please check repodata source. Exiting." << std::endl;
                    throw std::runtime_error(ss.str());
                }

                Console::stream() << termcolor::yellow << "Could not load repodata.json for "
                                  << subdir->name() << ". Deleting cache, and retrying."
                                  << termcolor::reset;
                subdir->clear_cache();
                loading_failed = true;
            }
        }

        if (loading_failed)
        {
            if (!ctx.offline && !(is_retry & RETRY_SUBDIR_FETCH))
            {
                LOG_WARNING << "Encountered malformed repodata.json cache. Redownloading.";
                return install_specs(specs, create_env, solver_flag, is_retry | RETRY_SUBDIR_FETCH);
            }
            throw std::runtime_error("Could not load repodata. Cache corrupted?");
        }

        MSolver solver(pool,
                       { { SOLVER_FLAG_ALLOW_DOWNGRADE, 1 },
                         { SOLVER_FLAG_STRICT_REPO_PRIORITY,
                           ctx.channel_priority == ChannelPriority::kStrict } });

        if (ctx.freeze_installed && !prefix_pkgs.empty())
        {
            LOG_INFO << "Locking environment: " << prefix_pkgs.size() << " packages freezed";
            solver.add_jobs(prefix_pkgs, SOLVER_LOCK);
        }

        solver.add_jobs(specs, solver_flag);

        if (!no_pin)
        {
            solver.add_pins(file_pins(prefix_data.path() / "conda-meta" / "pinned"));
            solver.add_pins(ctx.pinned_packages);
        }

        if (!no_py_pin)
        {
            auto py_pin = python_pin(prefix_data, specs);
            if (!py_pin.empty())
            {
                solver.add_pin(py_pin);
            }
        }
        if (!solver.pinned_specs().empty())
        {
            std::vector<std::string> pinned_str;
            for (auto& ms : solver.pinned_specs())
                pinned_str.push_back("  - " + ms.conda_build_form() + "\n");
            Console::print("\nPinned packages:\n" + join("", pinned_str));
        }

        bool success = solver.solve();
        if (!success)
        {
            Console::stream() << solver.problems_to_str();
            if (retry_clean_cache && !(is_retry & RETRY_SOLVE_ERROR))
            {
                ctx.local_repodata_ttl = 2;
                return install_specs(specs, create_env, solver_flag, is_retry | RETRY_SOLVE_ERROR);
            }
            if (ctx.freeze_installed)
                Console::print("Possible hints:\n  - 'freeze_installed' is turned on\n");

            throw std::runtime_error("Could not solve for environment specs");
        }

        MTransaction trans(solver, package_caches);

        if (ctx.json)
        {
            trans.log_json();
        }
        // TODO this is not so great
        std::vector<MRepo*> repo_ptrs;
        for (auto& r : repos)
        {
            repo_ptrs.push_back(&r);
        }

        Console::stream();

        bool yes = trans.prompt(repo_ptrs);
        if (yes)
        {
            if (create_env && !Context::instance().dry_run)
                detail::create_target_directory(ctx.target_prefix);

            trans.execute(prefix_data);

            for (auto other_spec : config.at("others_pkg_mgrs_specs")
                                       .value<std::vector<detail::other_pkg_mgr_spec>>())
            {
                install_for_other_pkgmgr(other_spec);
            }
        }
    }


    void install_explicit_specs(const std::vector<std::string>& specs, bool create_env)
    {
        MPool pool;
        auto& ctx = Context::instance();
        PrefixData prefix_data(ctx.target_prefix);
        fs::path pkgs_dirs(Context::instance().root_prefix / "pkgs");
        MultiPackageCache pkg_caches({ pkgs_dirs });
        auto transaction = create_explicit_transaction_from_urls(pool, specs, pkg_caches);

        prefix_data.load();
        prefix_data.add_virtual_packages(get_virtual_packages());
        MRepo(pool, prefix_data);

        std::vector<MRepo*> repo_ptrs;

        if (ctx.json)
            transaction.log_json();

        bool yes = transaction.prompt(repo_ptrs);
        if (yes)
        {
            if (create_env && !Context::instance().dry_run)
                detail::create_target_directory(ctx.target_prefix);

            transaction.execute(prefix_data);
        }
    }

    namespace detail
    {
        void create_empty_target(const fs::path& prefix)
        {
            detail::create_target_directory(prefix);

            Console::print(join(
                "", std::vector<std::string>({ "Empty environment created at prefix: ", prefix })));
            JsonLogger::instance().json_write({ { "success", true } });

            if (Context::instance().json)
                Console::instance().print(JsonLogger::instance().json_log.unflatten().dump(4),
                                          true);
        }

        void create_target_directory(const fs::path prefix)
        {
            path::touch(prefix / "conda-meta" / "history", true);
        }

        void file_specs_hook(std::vector<std::string>& file_specs)
        {
            auto& config = Configuration::instance();
            auto& env_name = config.at("spec_file_env_name");
            auto& specs = config.at("specs");
            auto& others_pkg_mgrs_specs = config.at("others_pkg_mgrs_specs");
            auto& channels = config.at("channels");

            if (file_specs.size() == 0)
                return;

            for (auto& file : file_specs)
            {
                if ((ends_with(file, ".yml") || ends_with(file, ".yaml")) && file_specs.size() != 1)
                {
                    throw std::runtime_error("Can only handle 1 yaml file!");
                }
            }

            for (auto& file : file_specs)
            {
                // read specs from file :)
                if (ends_with(file, ".yml") || ends_with(file, ".yaml"))
                {
                    auto parse_result = read_yaml_file(file);

                    if (parse_result.channels.size() != 0)
                    {
                        YAML::Node updated_channels;
                        if (channels.cli_configured())
                        {
                            updated_channels = channels.cli_yaml_value();
                        }
                        for (auto& c : parse_result.channels)
                        {
                            updated_channels.push_back(c);
                        }
                        channels.set_cli_yaml_value(updated_channels);
                    }

                    if (parse_result.name.size() != 0)
                    {
                        env_name.set_cli_yaml_value(parse_result.name);
                    }

                    if (parse_result.dependencies.size() != 0)
                    {
                        YAML::Node updated_specs;
                        if (specs.cli_configured())
                        {
                            updated_specs = specs.cli_yaml_value();
                        }
                        for (auto& s : parse_result.dependencies)
                        {
                            updated_specs.push_back(s);
                        }
                        specs.set_cli_yaml_value(updated_specs);
                    }

                    others_pkg_mgrs_specs.set_value(parse_result.others_pkg_mgrs_specs);
                }
                else
                {
                    std::vector<std::string> file_contents = read_lines(file);
                    if (file_contents.size() == 0)
                    {
                        throw std::runtime_error(concat("Got an empty file: ", file));
                    }
                    for (std::size_t i = 0; i < file_contents.size(); ++i)
                    {
                        auto& line = file_contents[i];
                        if (starts_with(line, "@EXPLICIT"))
                        {
                            // this is an explicit env
                            // we can check if the platform is correct with the previous line
                            std::string platform;
                            if (i >= 1)
                            {
                                for (std::size_t j = 0; j < i; ++j)
                                {
                                    platform = file_contents[j];
                                    if (starts_with(platform, "# platform: "))
                                    {
                                        platform = platform.substr(12);
                                        break;
                                    }
                                }
                            }
                            LOG_INFO << "Installing explicit specs for platform " << platform;

                            std::vector<std::string> explicit_specs;
                            for (auto f = file_contents.begin() + i + 1; f != file_contents.end();
                                 ++f)
                            {
                                std::string_view spec = strip((*f));
                                if (!spec.empty() && spec[0] != '#')
                                    explicit_specs.push_back(*f);
                            }

                            specs.clear_values();
                            specs.set_value(explicit_specs);
                            config.at("explicit_install").set_value(true);

                            return;
                        }
                    }

                    std::vector<std::string> f_specs;
                    for (auto& line : file_contents)
                    {
                        if (line[0] != '#' && line[0] != '@')
                        {
                            f_specs.push_back(line);
                        }
                    }

                    auto& s = specs.as<std::vector<std::string>>();
                    if (specs.cli_configured())
                    {
                        auto current_specs = s.cli_value();
                        current_specs.insert(current_specs.end(), f_specs.cbegin(), f_specs.cend());
                        s.set_cli_value(current_specs);
                    }
                    else
                    {
                        s.set_cli_config(f_specs);
                    }
                }
            }
        }

        MRepo create_repo_from_pkgs_dir(MPool& pool, const fs::path& pkgs_dir)
        {
            if (!fs::exists(pkgs_dir))
            {
                throw std::runtime_error("Specified pkgs_dir does not exist\n");
            }
            PrefixData prefix_data(pkgs_dir);
            prefix_data.load();
            for (const auto& entry : fs::directory_iterator(pkgs_dir))
            {
                fs::path repodata_record_json = entry.path() / "info" / "repodata_record.json";
                if (!fs::exists(repodata_record_json))
                {
                    continue;
                }
                prefix_data.load_single_record(repodata_record_json);
            }
            return MRepo(pool, prefix_data);
        }
    }  // detail
}  // mamba
