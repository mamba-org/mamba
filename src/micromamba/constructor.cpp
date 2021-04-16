// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"
#include "constructor.hpp"

#include "mamba/api/configuration.hpp"

#include "mamba/core/package_handling.hpp"
#include "mamba/core/util.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_constructor_parser(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& prefix = config.insert(Configurable("constructor_prefix", fs::path(""))
                                     .group("cli")
                                     .description("Extract the conda pkgs in <prefix>/pkgs"));

    subcom->add_option("-p,--prefix", prefix.set_cli_config(""), prefix.description());

    auto& extract_conda_pkgs
        = config.insert(Configurable("constructor_extract_conda_pkgs", false)
                            .group("cli")
                            .description("Extract the conda pkgs in <prefix>/pkgs"));
    subcom->add_flag("--extract-conda-pkgs",
                     extract_conda_pkgs.set_cli_config(0),
                     extract_conda_pkgs.description());

    auto& extract_tarball = config.insert(Configurable("constructor_extract_tarball", false)
                                              .group("cli")
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


void
construct(const fs::path& prefix, bool extract_conda_pkgs, bool extract_tarball)
{
    auto& config = Configuration::instance();

    config.at("show_banner").set_value(false);
    config.at("use_target_prefix_fallback").set_value(true);
    config.at("target_prefix_checks")
        .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX);
    config.load();

    if (extract_conda_pkgs)
    {
        fs::path pkgs_dir = prefix / "pkgs";
        fs::path filename;
        for (const auto& entry : fs::directory_iterator(pkgs_dir))
        {
            filename = entry.path().filename();
            if (ends_with(filename.string(), ".tar.bz2") || ends_with(filename.string(), ".conda"))
            {
                extract(entry.path());
            }
        }
    }
    if (extract_tarball)
    {
        fs::path extract_tarball_path = prefix / "_tmp.tar.bz2";
        read_binary_from_stdin_and_write_to_file(extract_tarball_path);
        extract_archive(extract_tarball_path, prefix);
        fs::remove(extract_tarball_path);
    }
}


void
read_binary_from_stdin_and_write_to_file(fs::path& filename)
{
    std::ofstream out_stream(filename.string().c_str(), std::ofstream::binary);
    // Need to reopen stdin as binary
    std::freopen(nullptr, "rb", stdin);
    if (std::ferror(stdin))
    {
        throw std::runtime_error("Re-opening stdin as binary failed.");
    }
    std::size_t len;
    std::array<char, 1024> buffer;

    while ((len = std::fread(buffer.data(), sizeof(char), buffer.size(), stdin)) > 0)
    {
        if (std::ferror(stdin) && !std::feof(stdin))
        {
            throw std::runtime_error("Reading from stdin failed.");
        }
        out_stream.write(buffer.data(), len);
    }
    out_stream.close();
}
