// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "clean.hpp"
#include "parsers.hpp"
#include "options.hpp"

#include "mamba/output.hpp"
#include "mamba/package_cache.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_clean_parser(CLI::App* subcom)
{
    init_general_options(subcom);

    subcom->add_flag("-a,--all",
                     clean_options.all,
                     "Remove index cache, lock files, unused cache packages, and tarballs.");
    subcom->add_flag("-i,--index-cache", clean_options.index_cache, "Remove index cache.");
    subcom->add_flag("-p,--packages",
                     clean_options.packages,
                     "Remove unused packages from writable package caches.\n"
                     "WARNING: This does not check for packages installed using\n"
                     "symlinks back to the package cache.");
    subcom->add_flag("-t,--tarballs", clean_options.tarballs, "Remove cached package tarballs.");
}

void
load_clean_options(Context& ctx)
{
    load_general_options(ctx);
    load_prefix_options(ctx);
    load_rc_options(ctx);
}

void
set_clean_command(CLI::App* subcom)
{
    init_clean_parser(subcom);

    subcom->callback([&]() {
        auto& ctx = Context::instance();

        load_clean_options(ctx);

        Console::print("Cleaning up ... ");

        std::vector<fs::path> envs;

        MultiPackageCache caches(ctx.pkgs_dirs);
        if (!ctx.dry_run && (clean_options.index_cache || clean_options.all))
        {
            for (auto* pkg_cache : caches.writable_caches())
                if (fs::exists(pkg_cache->get_pkgs_dir() / "cache"))
                {
                    try
                    {
                        fs::remove_all(pkg_cache->get_pkgs_dir() / "cache");
                    }
                    catch (...)
                    {
                        LOG_WARNING << "Could not clean " << pkg_cache->get_pkgs_dir() / "cache";
                    }
                }
        }

        if (fs::exists(ctx.root_prefix / "conda-meta"))
        {
            envs.push_back(ctx.root_prefix);
        }

        if (fs::exists(ctx.root_prefix / "envs"))
        {
            for (auto& p : fs::directory_iterator(ctx.root_prefix / "envs"))
            {
                if (p.is_directory() && fs::exists(p.path() / "conda-meta"))
                {
                    LOG_INFO << "Found environment: " << p.path();
                    envs.push_back(p);
                }
            }
        }

        // globally, collect installed packages
        std::set<std::string> installed_pkgs;
        for (auto& env : envs)
        {
            for (auto& pkg : fs::directory_iterator(env / "conda-meta"))
            {
                if (ends_with(pkg.path().string(), ".json"))
                {
                    std::string pkg_name = pkg.path().filename().string();
                    installed_pkgs.insert(pkg_name.substr(0, pkg_name.size() - 5));
                }
            }
        }

        auto get_file_size = [](const auto& s) -> std::string {
            std::stringstream ss;
            to_human_readable_filesize(ss, s);
            return ss.str();
        };

        auto collect_tarballs = [&]() {
            std::vector<fs::path> res;
            std::size_t total_size = 0;
            std::vector<printers::FormattedString> header = { "Package file", "Size" };
            mamba::printers::Table t(header);
            t.set_alignment({ printers::alignment::left, printers::alignment::right });
            t.set_padding({ 2, 4 });

            for (auto* pkg_cache : caches.writable_caches())
            {
                std::string header_line
                    = concat("Package cache folder: ", pkg_cache->get_pkgs_dir().string());
                std::vector<std::vector<printers::FormattedString>> rows;
                for (auto& p : fs::directory_iterator(pkg_cache->get_pkgs_dir()))
                {
                    std::string fname = p.path().filename();
                    if (!p.is_directory()
                        && (ends_with(p.path().string(), ".tar.bz2")
                            || ends_with(p.path().string(), ".conda")))
                    {
                        res.push_back(p.path());
                        rows.push_back(
                            { p.path().filename().string(), get_file_size(p.file_size()) });
                        total_size += p.file_size();
                    }
                }
                std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
                    return a[0].s < b[0].s;
                });
                t.add_rows(pkg_cache->get_pkgs_dir().string(), rows);
            }
            if (total_size)
            {
                t.add_rows({}, { { "Total size: ", get_file_size(total_size) } });
                t.print(std::cout);
            }
            return res;
        };

        if (clean_options.all || clean_options.tarballs)
        {
            auto to_be_removed = collect_tarballs();
            if (to_be_removed.size() == 0)
            {
                Console::print("No cached tarballs found");
            }
            else if (!ctx.dry_run && Console::prompt("\n\nRemove tarballs", 'y'))
            {
                for (auto& tbr : to_be_removed)
                {
                    fs::remove(tbr);
                }
                Console::print("\n\n");
            }
        }

        auto get_folder_size = [](auto& p) {
            std::size_t size = 0;
            for (auto& fp : fs::recursive_directory_iterator(p))
            {
                if (!fp.is_symlink())
                {
                    size += fp.file_size();
                }
            }
            return size;
        };

        auto collect_package_folders = [&]() {
            std::vector<fs::path> res;
            std::size_t total_size = 0;
            std::vector<printers::FormattedString> header = { "Package folder", "Size" };
            mamba::printers::Table t(header);
            t.set_alignment({ printers::alignment::left, printers::alignment::right });
            t.set_padding({ 2, 4 });

            for (auto* pkg_cache : caches.writable_caches())
            {
                std::string header_line
                    = concat("Package cache folder: ", pkg_cache->get_pkgs_dir().string());
                std::vector<std::vector<printers::FormattedString>> rows;
                for (auto& p : fs::directory_iterator(pkg_cache->get_pkgs_dir()))
                {
                    std::string fname = p.path().filename();
                    if (p.is_directory() && fs::exists(p.path() / "info" / "index.json"))
                    {
                        if (installed_pkgs.find(p.path().filename()) != installed_pkgs.end())
                        {
                            // do not remove installed packages
                            continue;
                        }
                        res.push_back(p.path());
                        std::size_t folder_size = get_folder_size(p);
                        rows.push_back(
                            { p.path().filename().string(), get_file_size(folder_size) });
                        total_size += folder_size;
                    }
                }
                std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
                    return a[0].s < b[0].s;
                });
                t.add_rows(pkg_cache->get_pkgs_dir().string(), rows);
            }
            if (total_size)
            {
                t.add_rows({}, { { "Total size: ", get_file_size(total_size) } });
                t.print(std::cout);
            }
            return res;
        };

        if (clean_options.all || clean_options.packages)
        {
            auto to_be_removed = collect_package_folders();
            if (to_be_removed.size() == 0)
            {
                Console::print("No cached tarballs found");
            }
            else if (!ctx.dry_run && Console::prompt("\n\nRemove unused packages", 'y'))
            {
                for (auto& tbr : to_be_removed)
                {
                    fs::remove_all(tbr);
                }
            }
        }
    });
}
