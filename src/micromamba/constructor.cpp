// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "constructor.hpp"
#include "common_options.hpp"

#include "mamba/configuration.hpp"
#include "mamba/package_handling.hpp"
#include "mamba/util.hpp"


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
        auto& config = Configuration::instance();

        config.at("show_banner").get_wrapped<bool>().set_value(false);
        config.load(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                    | MAMBA_ALLOW_EXISTING_PREFIX);

        fs::path prefix = config.at("constructor_prefix").value<fs::path>();
        auto& extract_conda_pkgs = config.at("constructor_extract_conda_pkgs").value<bool>();
        auto& extract_tarball = config.at("constructor_extract_tarball").value<bool>();

        if (extract_conda_pkgs)
        {
            fs::path pkgs_dir = prefix / "pkgs";
            fs::path filename;
            for (const auto& entry : fs::directory_iterator(pkgs_dir))
            {
                filename = entry.path().filename();
                if (ends_with(filename.string(), ".tar.bz2")
                    || ends_with(filename.string(), ".conda"))
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
        return 0;
    });
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
