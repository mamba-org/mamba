// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/clean.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_clean_parser(CLI::App* subcom)
{
    init_general_options(subcom);

    auto& config = Configuration::instance();

    auto& clean_all = config.insert(
        Configurable("clean_all", false)
            .group("cli")
            .description("Remove index cache, lock files, unused cache packages, and tarballs"));
    auto& clean_index = config.insert(
        Configurable("clean_index_cache", false).group("cli").description("Remove index cache"));
    auto& clean_pkgs
        = config.insert(Configurable("clean_packages", false)
                            .group("cli")
                            .description("Remove unused packages from writable package caches"));
    auto& clean_tarballs = config.insert(Configurable("clean_tarballs", false)
                                             .group("cli")
                                             .description("Remove cached package tarballs"));
    auto& clean_locks = config.insert(Configurable("clean_locks", false)
                                          .group("cli")
                                          .description("Remove lock files from caches"));
    auto& clean_trash
        = config.insert(Configurable("clean_trash", false)
                            .group("cli")
                            .description("Remove *.mamba_trash files from all environments"));

    auto& clean_force_pkgs_dirs
        = config.insert(Configurable("clean_force_pkgs_dirs", false)
                            .group("cli")
                            .description("Remove *all* writable package caches. This option is not included with the --all flags."));

    subcom->add_flag("-a,--all", clean_all.set_cli_config(0), clean_all.description());
    subcom->add_flag("-i,--index-cache", clean_index.set_cli_config(0), clean_index.description());
    subcom->add_flag("-p,--packages", clean_pkgs.set_cli_config(0), clean_pkgs.description());
    subcom->add_flag(
        "-t,--tarballs", clean_tarballs.set_cli_config(0), clean_tarballs.description());
    subcom->add_flag("-l,--locks", clean_locks.set_cli_config(0), clean_locks.description());
    subcom->add_flag("--trash", clean_trash.set_cli_config(0), clean_trash.description());
    subcom->add_flag("-f,--force-pkgs-dirs", clean_force_pkgs_dirs.set_cli_config(0), clean_force_pkgs_dirs.description());
}

void
set_clean_command(CLI::App* subcom)
{
    init_clean_parser(subcom);

    subcom->callback([&]() {
        auto& config = Configuration::instance();
        int options = 0;

        if (config.at("clean_all").compute().value<bool>())
            options = options | MAMBA_CLEAN_ALL;
        if (config.at("clean_index_cache").compute().value<bool>())
            options = options | MAMBA_CLEAN_INDEX;
        if (config.at("clean_packages").compute().value<bool>())
            options = options | MAMBA_CLEAN_PKGS;
        if (config.at("clean_tarballs").compute().value<bool>())
            options = options | MAMBA_CLEAN_TARBALLS;
        if (config.at("clean_locks").compute().value<bool>())
            options = options | MAMBA_CLEAN_LOCKS;
        if (config.at("clean_trash").compute().value<bool>())
            options = options | MAMBA_CLEAN_TRASH;
        if (config.at("clean_force_pkgs_dirs").compute().value<bool>())
        {
            if (Console::prompt("Remove all contents from the package caches?"))
            {
                options = options | MAMBA_CLEAN_FORCE_PKGS_DIRS;
            }
        }

        clean(options);
    });
}
