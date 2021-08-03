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

#include "thirdparty/termcolor.hpp"

namespace mamba
{
    static std::vector<std::tuple<std::string, std::vector<std::string>>> other_pkg_mgr_specs;
    static std::map<std::string, std::string> other_pkg_mgr_install_instructions
        = { { "pip", "pip install -r {0} --no-input" } };

    auto install_for_other_pkgmgr(const std::string& pkg_mgr, const std::vector<std::string>& deps)
    {
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
        std::string cwd = ctx.target_prefix;
        options.working_directory = cwd.c_str();

        std::cout << "\n"
                  << termcolor::cyan << "Installing " << pkg_mgr
                  << " packages: " << join(", ", deps) << termcolor::reset << std::endl;
        LOG_INFO << "Calling: " << join(" ", install_args);

        auto [_, ec] = reproc::run(wrapped_command, options);

        if (ec)
        {
            throw std::runtime_error(ec.message());
        }
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

            std::vector<std::string> pip_deps;
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
                            result.other_pkg_mgr_specs.push_back(std::make_tuple(
                                std::string("pip"), map_el.second.as<std::vector<std::string>>()));
                            pip_deps = map_el.second.as<std::vector<std::string>>();
                        }
                    }
                }
            }

            std::vector<std::string> dependencies = final_deps.as<std::vector<std::string>>();

            if (!pip_deps.empty())
            {
                if (!std::count(dependencies.begin(), dependencies.end(), "pip"))
                {
                    dependencies.push_back("pip");
                }
            }
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
                install_explicit_specs(install_specs);
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

        fs::path pkgs_dirs = ctx.pkgs_dirs.at(0);

        if (ctx.target_prefix.empty())
        {
            throw std::runtime_error("No active target prefix");
        }
        if (!fs::exists(ctx.target_prefix) && create_env == false)
        {
            LOG_ERROR << "Prefix does not exist at: " << ctx.target_prefix;
            exit(1);
        }

        fs::path cache_dir = pkgs_dirs / "cache";
        try
        {
            fs::create_directories(cache_dir);
        }
        catch (...)
        {
            throw std::runtime_error("Could not create `pkgs/cache/` dirs");
        }

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
        std::unique_ptr<LockFile> subdir_download_lock;
        if (!ctx.offline)
        {
            subdir_download_lock = std::make_unique<LockFile>(cache_dir / "mamba.lock");
        }

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
                    cache_dir / cache_fn_url(repodata_full_url),
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
            subdir_download_lock.reset();
        }

        std::vector<MRepo> repos;
        MPool pool;
        if (ctx.offline)
        {
            LOG_INFO << "Creating repo from pkgs_dir for offline";
            repos.push_back(detail::create_repo_from_pkgs_dir(pool, pkgs_dirs));
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

                std::cout << termcolor::yellow << "Could not load repodata.json for "
                          << subdir->name() << ". Deleting cache, and retrying." << termcolor::reset
                          << std::endl;
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
            std::cout << solver.problems_to_str() << std::endl;
            if (retry_clean_cache && !(is_retry & RETRY_SOLVE_ERROR))
            {
                ctx.local_repodata_ttl = 2;
                return install_specs(specs, create_env, solver_flag, is_retry | RETRY_SOLVE_ERROR);
            }
            if (ctx.freeze_installed)
                Console::print("Possible hints:\n  - 'freeze_installed' is turned on\n");

            throw std::runtime_error("Could not solve for environment specs");
        }

        MultiPackageCache package_caches({ pkgs_dirs });
        MTransaction trans(solver, package_caches, pkgs_dirs);

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

        std::cout << std::endl;

        bool yes = trans.prompt(repo_ptrs);
        if (yes)
        {
            if (create_env && !Context::instance().dry_run)
            {
                detail::create_target_directory(ctx.target_prefix);
            }
            {
                LockFile(pkgs_dirs / "mamba.lock");
                trans.execute(prefix_data);
            }
            for (const auto& [pkg_mgr, deps] : other_pkg_mgr_specs)
            {
                mamba::install_for_other_pkgmgr(pkg_mgr, deps);
            }
        }
    }


    void install_explicit_specs(const std::vector<std::string>& specs)
    {
        mamba::History hist(Context::instance().target_prefix);
        auto hist_entry = History::UserRequest::prefilled();
        std::string python_version;  // for pyc compilation
        // TODO unify this

        auto [pkg_infos, match_specs] = detail::parse_urls_to_package_info(specs);
        for (auto& ms : match_specs)
        {
            hist_entry.update.push_back(ms.str());
            if (ms.name == "python")
            {
                python_version = ms.version;
            }
        }

        if (!Context::instance().dry_run && detail::download_explicit(pkg_infos))
        {
            auto& ctx = Context::instance();
            // pkgs can now be linked
            fs::create_directories(ctx.target_prefix / "conda-meta");

            TransactionContext tctx(ctx.target_prefix, python_version);
            for (auto& pkg : pkg_infos)
            {
                LinkPackage lp(pkg, ctx.root_prefix / "pkgs", &tctx);
                std::cout << "Linking " << pkg.str() << "\n";
                hist_entry.link_dists.push_back(pkg.long_str());
                lp.execute();
            }

            hist.add_entry(hist_entry);
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

                    if (parse_result.other_pkg_mgr_specs.size())
                    {
                        other_pkg_mgr_specs = parse_result.other_pkg_mgr_specs;
                    }
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

        bool download_explicit(const std::vector<PackageInfo>& pkgs)
        {
            fs::path pkgs_dirs(Context::instance().root_prefix / "pkgs");
            // fs::path pkgs_dirs;
            // if (std::getenv("CONDA_PKGS_DIRS") != nullptr)
            // {
            //     pkgs_dirs = fs::path(std::getenv("CONDA_PKGS_DIRS"));
            // }
            // else
            // {
            //     pkgs_dirs = ctx.root_prefix / "pkgs";
            // }

            // TODO better error handling for checking that cache path is
            // directory and writable etc.
            if (!fs::exists(pkgs_dirs))
            {
                fs::create_directories(pkgs_dirs);
            }

            LockFile(pkgs_dirs / "mamba.lock");

            std::vector<std::unique_ptr<PackageDownloadExtractTarget>> targets;
            MultiDownloadTarget multi_dl;
            Console::instance().init_multi_progress(ProgressBarMode::aggregated);
            MultiPackageCache pkg_cache({ Context::instance().root_prefix / "pkgs" });

            for (auto& pkg : pkgs)
            {
                targets.emplace_back(std::make_unique<PackageDownloadExtractTarget>(pkg));
                multi_dl.add(targets[targets.size() - 1]->target(pkgs_dirs, pkg_cache));
            }

            interruption_guard g([]() { Console::instance().init_multi_progress(); });

            bool downloaded = multi_dl.download(true);

            if (!downloaded)
            {
                LOG_ERROR << "Download didn't finish!";
                return false;
            }
            // make sure that all targets have finished extracting
            while (!is_sig_interrupted())
            {
                bool all_finished = true;
                for (const auto& t : targets)
                {
                    if (!t->finished())
                    {
                        all_finished = false;
                        break;
                    }
                }
                if (all_finished)
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            return !is_sig_interrupted() && downloaded;
        }
    }  // detail
}  // mamba
