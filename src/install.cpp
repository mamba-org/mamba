// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/install.hpp"
#include "mamba/channel.hpp"
#include "mamba/configuration.hpp"
#include "mamba/link.hpp"
#include "mamba/mamba_fs.hpp"
#include "mamba/output.hpp"
#include "mamba/package_cache.hpp"
#include "mamba/pinning.hpp"
#include "mamba/subdirdata.hpp"
#include "mamba/thread_utils.hpp"
#include "mamba/transaction.hpp"
#include "mamba/virtual_packages.hpp"

#include "thirdparty/termcolor.hpp"


namespace mamba
{
    void install(const std::vector<std::string>& specs, const fs::path& prefix)
    {
        auto& ctx = Context::instance();

        if (!prefix.empty())
            ctx.target_prefix = prefix;

        if (!specs.empty())
        {
            using namespace detail;

            if (!check_target_prefix(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                                     | MAMBA_ALLOW_EXISTING_PREFIX))
                install_specs(specs, false);
        }
        else
        {
            Console::print("Nothing to do.");
        }
    }

    namespace detail
    {
        int RETRY_SUBDIR_FETCH = 1 << 0;
        int RETRY_SOLVE_ERROR = 1 << 1;

        void
        install_specs(const std::vector<std::string>& specs, bool create_env, int solver_flag, int is_retry)
        {
            auto& ctx = Context::instance();
            auto& config = Configuration::instance();

            auto& no_pin = config.at("no_pin").value<bool>();
            auto& retry_clean_cache = config.at("retry_clean_cache").value<bool>();

            fs::path pkgs_dirs;

            if (std::getenv("CONDA_PKGS_DIRS") != nullptr)
            {
                pkgs_dirs = fs::path(std::getenv("CONDA_PKGS_DIRS"));
            }
            else
            {
                pkgs_dirs = ctx.root_prefix / "pkgs";
            }

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

            if (ctx.channels.empty() && !ctx.offline)
            {
                LOG_WARNING << "No 'channels' specified";
            }

            std::vector<std::string> channel_urls = calculate_channel_urls(ctx.channels);

            std::vector<std::shared_ptr<MSubdirData>> subdirs;
            MultiDownloadTarget multi_dl;

            std::vector<std::pair<int, int>> priorities;
            int max_prio = static_cast<int>(channel_urls.size());
            std::string prev_channel_name;
            for (auto& url : channel_urls)
            {
                auto& channel = make_channel(url);
                std::string full_url = concat(channel.url(true), "/repodata.json");

                auto sdir = std::make_shared<MSubdirData>(concat(channel.name(), "/", channel.platform()),
                                                        full_url,
                                                        cache_dir / cache_fn_url(full_url));

                sdir->load();
                multi_dl.add(sdir->target());
                subdirs.push_back(sdir);
                if (ctx.channel_priority == ChannelPriority::kDisabled)
                {
                    priorities.push_back(std::make_pair(0, max_prio--));
                }
                else  // Consider 'flexible' and 'strict' the same way
                {
                    if (channel.name() != prev_channel_name)
                    {
                        max_prio--;
                        prev_channel_name = channel.name();
                    }
                    priorities.push_back(std::make_pair(max_prio, channel.platform() == "noarch" ? 0 : 1));
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
                repos.push_back(create_repo_from_pkgs_dir(pool, pkgs_dirs));
            }
            PrefixData prefix_data(ctx.target_prefix);
            prefix_data.load();
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
                    if (ctx.offline || mamba::ends_with(subdir->name(), "/noarch"))
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

                    std::cout << termcolor::yellow << "Could not load repodata.json for " << subdir->name()
                            << ". Deleting cache, and retrying." << termcolor::reset << std::endl;
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

            MSolver solver(pool, { { SOLVER_FLAG_ALLOW_DOWNGRADE, 1 } });
            solver.add_jobs(specs, solver_flag);

            if (!no_pin)
            {
                solver.add_pins(file_pins(prefix_data.path() / "conda-meta" / "pinned"));
                solver.add_pins(ctx.pinned_packages);
            }

            auto py_pin = python_pin(prefix_data, specs);
            if (!py_pin.empty())
            {
                solver.add_pin(py_pin);
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
                throw std::runtime_error("Could not solve for environment specs");
            }

            MultiPackageCache package_caches({ pkgs_dirs });
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

            std::cout << std::endl;

            bool yes = trans.prompt(pkgs_dirs, repo_ptrs);
            if (!yes)
                exit(0);

            if (create_env && !Context::instance().dry_run)
            {
                fs::create_directories(ctx.target_prefix / "conda-meta");
                fs::create_directories(ctx.target_prefix / "pkgs");
            }

            trans.execute(prefix_data, pkgs_dirs);
        }

        void
        parse_file_options()
        {
            auto& configuration = Configuration::instance();
            auto& file_specs
                = configuration.at("file_specs").compute_config().value<std::vector<std::string>>();
            auto& env_name = configuration.at("env_name");
            auto& specs = configuration.at("specs");
            auto& channels = configuration.at("channels");

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
                    YAML::Node config;
                    try
                    {
                        config = YAML::LoadFile(file);
                    }
                    catch (YAML::Exception& e)
                    {
                        LOG_ERROR << "Error in spec file: " << file;
                        continue;
                    }

                    if (config["channels"])
                    {
                        YAML::Node yaml_channels;
                        try
                        {
                            yaml_channels = config["channels"];
                        }
                        catch (YAML::Exception& e)
                        {
                            throw std::runtime_error(
                                mamba::concat("Could not read 'channels' as list of strings from ", file));
                        }

                        channels.add_rc_value(yaml_channels, file);
                    }
                    else
                    {
                        LOG_DEBUG << "No 'channels' specified in file: " << file;
                    }

                    if (config["name"])
                    {
                        env_name.add_rc_value(config["name"], file);
                    }
                    {
                        LOG_DEBUG << "No env 'name' specified in file: " << file;
                    }

                    if (config["dependencies"])
                    {
                        specs.add_rc_value(config["dependencies"], file);
                    }
                    else
                    {
                        throw std::runtime_error(concat("No 'dependencies' specified in file: ", file));
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

                            std::vector<std::string> explicit_specs(file_contents.begin() + i + 1,
                                                                    file_contents.end());

                            load_configuration(0);
                            install_explicit_specs(explicit_specs);
                            exit(0);
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
                    configuration.at("specs").add_rc_value(YAML::Node(f_specs), file);
                }
            }
        }

        MRepo
        create_repo_from_pkgs_dir(MPool& pool, const fs::path& pkgs_dir)
        {
            if (!fs::exists(pkgs_dir))
            {
                throw std::runtime_error("Specified pkgs_dir does not exist\n");
            }
            PrefixData prefix_data(pkgs_dir);
            prefix_data.load();
            for (const auto& entry : fs::directory_iterator(pkgs_dir))
            {
                fs::path info_json = entry.path() / "info" / "index.json";
                if (!fs::exists(info_json))
                {
                    continue;
                }
                prefix_data.load_single_record(info_json);
            }
            return MRepo(pool, prefix_data);
        }


        void
        install_explicit_specs(std::vector<std::string>& specs)
        {
            std::vector<mamba::MatchSpec> match_specs;
            std::vector<mamba::PackageInfo> pkg_infos;
            mamba::History hist(Context::instance().target_prefix);
            auto hist_entry = History::UserRequest::prefilled();
            std::string python_version;  // for pyc compilation
            // TODO unify this
            for (auto& spec : specs)
            {
                if (strip(spec).size() == 0)
                    continue;
                std::size_t hash = spec.find_first_of('#');
                match_specs.emplace_back(spec.substr(0, hash));
                auto& ms = match_specs.back();
                PackageInfo p(ms.name);
                p.url = ms.url;
                p.build_string = ms.build;
                p.version = ms.version;
                p.channel = ms.channel;
                p.fn = ms.fn;

                if (hash != std::string::npos)
                {
                    ms.brackets["md5"] = spec.substr(hash + 1);
                    p.md5 = spec.substr(hash + 1);
                }
                hist_entry.update.push_back(ms.str());
                pkg_infos.push_back(p);

                if (ms.name == "python")
                {
                    python_version = ms.version;
                }
            }
            if (download_explicit(pkg_infos))
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


        bool
        download_explicit(const std::vector<PackageInfo>& pkgs)
        {
            fs::path cache_path(Context::instance().root_prefix / "pkgs");
            // TODO better error handling for checking that cache path is
            // directory and writable etc.
            if (!fs::exists(cache_path))
            {
                fs::create_directories(cache_path);
            }
            std::vector<std::unique_ptr<PackageDownloadExtractTarget>> targets;
            MultiDownloadTarget multi_dl;
            MultiPackageCache pkg_cache({ Context::instance().root_prefix / "pkgs" });

            for (auto& pkg : pkgs)
            {
                targets.emplace_back(std::make_unique<PackageDownloadExtractTarget>(pkg));
                multi_dl.add(targets[targets.size() - 1]->target(cache_path, pkg_cache));
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

        int check_target_prefix(int options)
        {
            auto& ctx = Context::instance();
            auto& prefix = ctx.target_prefix;

            if (prefix.empty() && (options & MAMBA_ALLOW_FALLBACK_PREFIX))
            {
                prefix = std::getenv("CONDA_PREFIX") ? std::getenv("CONDA_PREFIX") : "";
            }

            if (prefix.empty())
            {
                LOG_ERROR << "No target prefix specified";
                return 1;
            }

            if ((prefix == ctx.root_prefix) && !(options & MAMBA_ALLOW_ROOT_PREFIX))
            {
                LOG_ERROR << "'root_prefix' not accepted as 'target_prefix'";
                return 1;
            }

            bool allow_existing = options & MAMBA_ALLOW_EXISTING_PREFIX;

            if (!allow_existing && fs::exists(prefix))
            {
                if (fs::exists(prefix / "conda-meta") || (prefix == ctx.root_prefix))
                {
                    if (!allow_existing)
                    {
                        if (Console::prompt("Found conda-prefix at '" + prefix.string()
                                                + "'.\nOverwrite?",
                                            'n'))
                        {
                            fs::remove_all(prefix);
                        }
                        else
                        {
                            return 1;
                        }
                    }
                }
                else
                {
                    LOG_ERROR << "Non-conda folder exists at prefix";
                    return 1;
                }
            }

            return 0;
        }
    }  // detail
}  // mamba
