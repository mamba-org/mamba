// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"
#include "constructor.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"

#include "mamba/core/package_handling.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/package_info.hpp"


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
        fs::path urls_file = pkgs_dir / "urls";

        auto [package_details, _] = detail::parse_urls_to_package_info(read_lines(urls_file));

        for (const auto& entry : fs::directory_iterator(pkgs_dir))
        {
            if (is_package_file(entry.path().filename().string()))
            {
                LOG_TRACE << "Extracting " << entry.path().filename() << std::endl;
                std::cout << "Extracting " << entry.path().filename() << std::endl;

                fs::path base_path = extract(entry.path());

                fs::path repodata_record_path = base_path / "info" / "repodata_record.json";
                fs::path index_path = base_path / "info" / "index.json";

                nlohmann::json index;
                std::ifstream index_file(index_path);
                index_file >> index;

                std::string pkg_name = index["name"];

                index["fn"] = entry.path().filename();
                bool found_match = false;
                for (const auto& pkg_info : package_details)
                {
                    if (pkg_info.fn == entry.path().filename())
                    {
                        index["url"] = pkg_info.url;
                        index["channel"] = pkg_info.channel;
                        index["size"] = fs::file_size(entry.path());
                        if (!pkg_info.md5.empty())
                        {
                            index["md5"] = pkg_info.md5;
                        }
                        if (!pkg_info.sha256.empty())
                        {
                            index["sha256"] = pkg_info.sha256;
                        }
                        found_match = true;
                        break;
                    }
                }
                if (!found_match)
                {
                    LOG_WARNING << "Failed to add extra info to " << repodata_record_path
                                << std::endl;
                }

                LOG_TRACE << "Writing " << repodata_record_path;
                std::ofstream repodata_record(repodata_record_path);
                repodata_record << index.dump(4);
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
    std::ofstream out_stream = open_ofstream(filename, std::ofstream::binary);
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
