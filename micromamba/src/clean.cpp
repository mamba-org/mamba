// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/clean.hpp"
#include "mamba/api/configuration.hpp"

#include "common_options.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_clean_parser(CLI::App* subcom, Configuration& config)
{
    init_general_options(subcom, config);

    auto& clean_all = config.insert(
        Configurable("clean_all", false)
            .group("cli")
            .description("Remove index cache, lock files, unused cache packages, and tarballs")
    );
    auto& clean_index = config.insert(
        Configurable("clean_index_cache", false).group("cli").description("Remove index cache")
    );
    auto& clean_pkgs = config.insert(
        Configurable("clean_packages", false)
            .group("cli")
            .description("Remove unused packages from writable package caches")
    );
    auto& clean_tarballs = config.insert(
        Configurable("clean_tarballs", false).group("cli").description("Remove cached package tarballs")
    );
    auto& clean_locks = config.insert(
        Configurable("clean_locks", false).group("cli").description("Remove lock files from caches")
    );
    auto& clean_trash = config.insert(
        Configurable("clean_trash", false)
            .group("cli")
            .description("Remove *.mamba_trash files from all environments")
    );

    auto& clean_force_pkgs_dirs = config.insert(
        Configurable("clean_force_pkgs_dirs", false)
            .group("cli")
            .description(
                "Remove *all* writable package caches. This option is not included with the --all flags."
            )
    );

    subcom->add_flag("-a,--all", clean_all.get_cli_config<bool>(), clean_all.description());
    subcom->add_flag("-i,--index-cache", clean_index.get_cli_config<bool>(), clean_index.description());
    subcom->add_flag("-p,--packages", clean_pkgs.get_cli_config<bool>(), clean_pkgs.description());
    subcom->add_flag(
        "-t,--tarballs",
        clean_tarballs.get_cli_config<bool>(),
        clean_tarballs.description()
    );
    subcom->add_flag("-l,--locks", clean_locks.get_cli_config<bool>(), clean_locks.description());
    subcom->add_flag("--trash", clean_trash.get_cli_config<bool>(), clean_trash.description());
    subcom->add_flag(
        "-f,--force-pkgs-dirs",
        clean_force_pkgs_dirs.get_cli_config<bool>(),
        clean_force_pkgs_dirs.description()
    );
}

void
set_clean_command(CLI::App* subcom, Configuration& config)
{
    init_clean_parser(subcom, config);

    subcom->callback(
        [&]()
        {
            int options = 0;

            if (config.at("clean_all").compute().value<bool>())
            {
                options = options | MAMBA_CLEAN_ALL;
            }
            if (config.at("clean_index_cache").compute().value<bool>())
            {
                options = options | MAMBA_CLEAN_INDEX;
            }
            if (config.at("clean_packages").compute().value<bool>())
            {
                options = options | MAMBA_CLEAN_PKGS;
            }
            if (config.at("clean_tarballs").compute().value<bool>())
            {
                options = options | MAMBA_CLEAN_TARBALLS;
            }
            if (config.at("clean_locks").compute().value<bool>())
            {
                options = options | MAMBA_CLEAN_LOCKS;
            }
            if (config.at("clean_trash").compute().value<bool>())
            {
                options = options | MAMBA_CLEAN_TRASH;
            }
            if (config.at("clean_force_pkgs_dirs").compute().value<bool>())
            {
                if (config.at("always_yes").compute().value<bool>()
                    || Console::prompt("Remove all contents from the package caches?"))
                {
                    options = options | MAMBA_CLEAN_FORCE_PKGS_DIRS;
                }
            }

            clean(config, options);
        }
    );
}
