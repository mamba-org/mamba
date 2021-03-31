// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "clean.hpp"
#include "common_options.hpp"

#include "mamba/core/configuration.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_clean_parser(CLI::App* subcom)
{
    init_general_options(subcom);

    auto& config = Configuration::instance();

    auto& clean_all = config.insert(
        Configurable("clean_all", false)
            .group("cli")
            .rc_configurable(false)
            .description("Remove index cache, lock files, unused cache packages, and tarballs"));
    auto& clean_index = config.insert(Configurable("clean_index_cache", false)
                                          .group("cli")
                                          .rc_configurable(false)
                                          .description("Remove index cache"));
    auto& clean_pkgs
        = config.insert(Configurable("clean_packages", false)
                            .group("cli")
                            .rc_configurable(false)
                            .description("Remove unused packages from writable package caches"));
    auto& clean_tarballs = config.insert(Configurable("clean_tarballs", false)
                                             .group("cli")
                                             .rc_configurable(false)
                                             .description("Remove cached package tarballs"));

    subcom->add_flag("-a,--all", clean_all.set_cli_config(0), clean_all.description());
    subcom->add_flag("-i,--index-cache", clean_index.set_cli_config(0), clean_index.description());
    subcom->add_flag("-p,--packages", clean_pkgs.set_cli_config(0), clean_pkgs.description());
    subcom->add_flag(
        "-t,--tarballs", clean_tarballs.set_cli_config(0), clean_tarballs.description());
}

void
set_clean_command(CLI::App* subcom)
{
    init_clean_parser(subcom);

    subcom->callback([&]() {
        load_configuration(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                           | MAMBA_ALLOW_EXISTING_PREFIX);

        auto& ctx = Context::instance();
        auto& config = Configuration::instance();

        auto& clean_all = config.at("clean_all").value<bool>();
        auto& clean_index = config.at("clean_index_cache").value<bool>();
        auto& clean_pkgs = config.at("clean_packages").value<bool>();
        auto& clean_tarballs = config.at("clean_tarballs").value<bool>();

        if (!(clean_all || clean_index || clean_pkgs || clean_tarballs))
        {
            std::cout << "Nothing to do." << std::endl;
            return;
        }

        Console::print("Collect information..");

        std::vector<fs::path> envs;

        MultiPackageCache caches(ctx.pkgs_dirs);
        if (!ctx.dry_run && (clean_index || clean_all))
        {
            Console::print("Cleaning index cache..");

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
                    LOG_DEBUG << "Found environment: " << p.path();
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

        if (clean_all || clean_tarballs)
        {
            auto to_be_removed = collect_tarballs();
            if (!ctx.dry_run)
            {
                Console::print("Cleaning tarballs..");

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

        if (clean_all || clean_pkgs)
        {
            auto to_be_removed = collect_package_folders();
            if (!ctx.dry_run)
            {
                Console::print("Cleaning packages..");

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
    });
}
