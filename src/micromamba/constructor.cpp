// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/construct.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_constructor_parser(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& prefix = config.insert(Configurable("constructor_prefix", fs::path(""))
                                     .group("cli")
                                     .rc_configurable(false)
                                     .description("Extract the conda pkgs in <prefix>/pkgs"));

    subcom->add_option("-p,--prefix", prefix.set_cli_config(""), prefix.description());

    auto& extract_conda_pkgs
        = config.insert(Configurable("constructor_extract_conda_pkgs", false)
                            .group("cli")
                            .rc_configurable(false)
                            .description("Extract the conda pkgs in <prefix>/pkgs"));
    subcom->add_flag("--extract-conda-pkgs",
                     extract_conda_pkgs.set_cli_config(0),
                     extract_conda_pkgs.description());

    auto& extract_tarball = config.insert(Configurable("constructor_extract_tarball", false)
                                              .group("cli")
                                              .rc_configurable(false)
                                              .description("Extract given tarball into prefix"));
    subcom->add_flag(
        "--extract-tarball", extract_tarball.set_cli_config(0), extract_tarball.description());
}

void
set_constructor_command(CLI::App* subcom)
{
    init_constructor_parser(subcom);

    subcom->callback([&]() {
        auto& c = Configuration::instance();

        auto& prefix = c.at("constructor_prefix").compute().value<fs::path>();
        auto& extract_conda_pkgs = c.at("constructor_extract_conda_pkgs").compute().value<bool>();
        auto& extract_tarball = c.at("constructor_extract_tarball").compute().value<bool>();

        construct(prefix, extract_conda_pkgs, extract_tarball);
    });
}
