// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include "mamba/api/clean.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/string.hpp"

#include "../core/progress_bar_impl.hpp"

namespace mamba
{
    void clean(Configuration& config, int options)
    {
        auto& ctx = Context::instance();

        config.at("use_target_prefix_fallback").set_value(true);
        config.load();

        bool clean_all = options & MAMBA_CLEAN_ALL;
        bool clean_index = options & MAMBA_CLEAN_INDEX;
        bool clean_pkgs = options & MAMBA_CLEAN_PKGS;
        bool clean_tarballs = options & MAMBA_CLEAN_TARBALLS;
        bool clean_locks = options & MAMBA_CLEAN_LOCKS;
        bool clean_trash = options & MAMBA_CLEAN_TRASH;
        bool clean_force_pkgs_dirs = options & MAMBA_CLEAN_FORCE_PKGS_DIRS;

        if (!(clean_all || clean_index || clean_pkgs || clean_tarballs || clean_locks || clean_trash
              || clean_force_pkgs_dirs))
        {
            Console::stream() << "Nothing to do." << std::endl;
            return;
        }

        Console::stream() << "Collect information..";

        std::vector<fs::u8path> envs;

        MultiPackageCache caches(ctx.pkgs_dirs);
        if (!ctx.dry_run && (clean_index || clean_all))
        {
            Console::stream() << "Cleaning index cache..";

            for (auto* pkg_cache : caches.writable_caches())
            {
                if (fs::exists(pkg_cache->path() / "cache"))
                {
                    try
                    {
                        fs::remove_all(pkg_cache->path() / "cache");
                    }
                    catch (...)
                    {
                        LOG_WARNING << "Could not clean " << pkg_cache->path() / "cache";
                    }
                }
            }
        }

        if (!ctx.dry_run && (clean_locks || clean_all))
        {
            Console::stream() << "Cleaning lock files..";

            for (auto* pkg_cache : caches.writable_caches())
            {
                if (fs::exists(pkg_cache->path()))
                {
                    for (auto& p : fs::directory_iterator(pkg_cache->path()))
                    {
                        if (p.exists() && util::ends_with(p.path().string(), ".lock")
                            && (fs::exists(util::rstrip(p.path().string(), ".lock"))
                                || (util::rstrip(p.path().filename().string(), ".lock")
                                    == p.path().parent_path().filename())))
                        {
                            try
                            {
                                LOG_INFO << "Removing lock file '" << p.path().string() << "'";
                                fs::remove(p);
                            }
                            catch (...)
                            {
                                LOG_WARNING << "Could not clean lock file '" << p.path().string()
                                            << "'";
                            }
                        }
                    }
                }

                if (fs::exists(pkg_cache->path() / "cache"))
                {
                    for (auto& p : fs::recursive_directory_iterator(pkg_cache->path() / "cache"))
                    {
                        if (p.exists() && util::ends_with(p.path().string(), ".lock"))
                        {
                            try
                            {
                                LOG_INFO << "Removing lock file '" << p.path().string() << "'";
                                fs::remove(p);
                            }
                            catch (...)
                            {
                                LOG_WARNING << "Could not clean lock file '" << p.path().string()
                                            << "'";
                            }
                        }
                    }
                }
            }
        }

        if (fs::exists(ctx.prefix_params.root_prefix / "conda-meta"))
        {
            envs.push_back(ctx.prefix_params.root_prefix);
        }

        if (fs::exists(ctx.prefix_params.root_prefix / "envs"))
        {
            for (auto& p : fs::directory_iterator(ctx.prefix_params.root_prefix / "envs"))
            {
                if (p.is_directory() && fs::exists(p.path() / "conda-meta"))
                {
                    LOG_DEBUG << "Found environment: " << p.path();
                    envs.push_back(p);
                }
            }
        }

        if (clean_trash)
        {
            Console::stream() << "Cleaning *.mamba_trash files" << std::endl;
            clean_trash_files(ctx.prefix_params.root_prefix, true);
        }

        // globally, collect installed packages
        std::set<std::string> installed_pkgs;
        for (auto& env : envs)
        {
            for (auto& pkg : fs::directory_iterator(env / "conda-meta"))
            {
                if (util::ends_with(pkg.path().string(), ".json"))
                {
                    std::string pkg_name = pkg.path().filename().string();
                    installed_pkgs.insert(pkg_name.substr(0, pkg_name.size() - 5));
                }
            }
        }

        auto get_file_size = [](const auto& s) -> std::string
        {
            std::stringstream ss;
            to_human_readable_filesize(ss, double(s));
            return ss.str();
        };

        auto collect_tarballs = [&]()
        {
            std::vector<fs::u8path> res;
            std::size_t total_size = 0;
            std::vector<printers::FormattedString> header = { "Package file", "Size" };
            mamba::printers::Table t(header);
            t.set_alignment({ printers::alignment::left, printers::alignment::right });
            t.set_padding({ 2, 4 });

            for (auto* pkg_cache : caches.writable_caches())
            {
                std::string header_line = util::concat(
                    "Package cache folder: ",
                    pkg_cache->path().string()
                );
                std::vector<std::vector<printers::FormattedString>> rows;
                for (auto& p : fs::directory_iterator(pkg_cache->path()))
                {
                    std::string fname = p.path().filename().string();
                    if (!p.is_directory()
                        && (util::ends_with(p.path().string(), ".tar.bz2")
                            || util::ends_with(p.path().string(), ".conda")))
                    {
                        res.push_back(p.path());
                        rows.push_back({ p.path().filename().string(), get_file_size(p.file_size()) });
                        total_size += p.file_size();
                    }
                }
                std::sort(
                    rows.begin(),
                    rows.end(),
                    [](const auto& a, const auto& b) { return a[0].s < b[0].s; }
                );
                t.add_rows(pkg_cache->path().string(), rows);
            }
            if (total_size)
            {
                t.add_rows({}, { { "Total size: ", get_file_size(total_size) } });
                t.print(std::cout);
            }
            return res;
        };

        if (clean_all || clean_tarballs)
        {
            auto to_be_removed = collect_tarballs();
            if (!ctx.dry_run)
            {
                Console::instance().print("Cleaning tarballs..");

                if (to_be_removed.size() == 0)
                {
                    LOG_INFO << "No cached tarballs found";
                }
                else if (!ctx.dry_run && Console::prompt("\nRemove tarballs", 'y'))
                {
                    for (auto& tbr : to_be_removed)
                    {
                        fs::remove(tbr);
                    }
                }
            }
        }

        auto get_folder_size = [](auto& p)
        {
            std::size_t size = 0;
            for (auto& fp : fs::recursive_directory_iterator(p))
            {
                if (!fp.is_symlink() && !fp.is_directory())
                {
                    size += fp.file_size();
                }
            }
            return size;
        };

        auto collect_package_folders = [&]()
        {
            std::vector<fs::u8path> res;
            std::size_t total_size = 0;
            std::vector<printers::FormattedString> header = { "Package folder", "Size" };
            mamba::printers::Table t(header);
            t.set_alignment({ printers::alignment::left, printers::alignment::right });
            t.set_padding({ 2, 4 });

            for (auto* pkg_cache : caches.writable_caches())
            {
                std::string header_line = util::concat(
                    "Package cache folder: ",
                    pkg_cache->path().string()
                );
                std::vector<std::vector<printers::FormattedString>> rows;
                for (auto& p : fs::directory_iterator(pkg_cache->path()))
                {
                    if (p.is_directory() && fs::exists(p.path() / "info" / "index.json"))
                    {
                        if (installed_pkgs.find(p.path().filename().string()) != installed_pkgs.end())
                        {
                            // do not remove installed packages
                            continue;
                        }
                        res.push_back(p.path());
                        std::size_t folder_size = get_folder_size(p);
                        rows.push_back({ p.path().filename().string(), get_file_size(folder_size) });
                        total_size += folder_size;
                    }
                }
                std::sort(
                    rows.begin(),
                    rows.end(),
                    [](const auto& a, const auto& b) { return a[0].s < b[0].s; }
                );
                t.add_rows(pkg_cache->path().string(), rows);
            }
            if (total_size)
            {
                t.add_rows({}, { { "Total size: ", get_file_size(total_size) } });
                t.print(std::cout);
            }
            return res;
        };

        if (clean_all || clean_pkgs)
        {
            auto to_be_removed = collect_package_folders();
            if (!ctx.dry_run)
            {
                Console::instance().print("Cleaning packages..");

                if (to_be_removed.size() == 0)
                {
                    LOG_INFO << "No cached packages found";
                }
                else
                {
                    LOG_WARNING << unindent(R"(
                            This does not check for packages installed using
                            symlinks back to the package cache.)");

                    if (Console::prompt("\nRemove unused packages", 'y'))
                    {
                        for (auto& tbr : to_be_removed)
                        {
                            fs::remove_all(tbr);
                        }
                    }
                }
            }
        }

        if (clean_force_pkgs_dirs)
        {
            for (auto* cache : caches.writable_caches())
            {
                fs::remove_all(cache->path());
            }
        }
    }
}  // mamba
