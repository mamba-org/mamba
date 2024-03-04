// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <CLI/App.hpp>

#include "constructor.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/string.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_constructor_parser(CLI::App* subcom, Configuration& config)
{
    auto& prefix = config.insert(Configurable("constructor_prefix", fs::u8path(""))
                                     .group("cli")
                                     .description("Extract the conda pkgs in <prefix>/pkgs"));

    subcom->add_option("-p,--prefix", prefix.get_cli_config<fs::u8path>(), prefix.description());

    auto& extract_conda_pkgs = config.insert(Configurable("constructor_extract_conda_pkgs", false)
                                                 .group("cli")
                                                 .description("Extract the conda pkgs in <prefix>/pkgs"
                                                 ));
    subcom->add_flag(
        "--extract-conda-pkgs",
        extract_conda_pkgs.get_cli_config<bool>(),
        extract_conda_pkgs.description()
    );

    auto& extract_tarball = config.insert(Configurable("constructor_extract_tarball", false)
                                              .group("cli")
                                              .description("Extract given tarball into prefix"));
    subcom->add_flag(
        "--extract-tarball",
        extract_tarball.get_cli_config<bool>(),
        extract_tarball.description()
    );
}

void
set_constructor_command(CLI::App* subcom, mamba::Configuration& config)
{
    init_constructor_parser(subcom, config);

    subcom->callback(
        [&config]
        {
            auto& prefix = config.at("constructor_prefix").compute().value<fs::u8path>();
            auto& extract_conda_pkgs = config.at("constructor_extract_conda_pkgs")
                                           .compute()
                                           .value<bool>();
            auto& extract_tarball = config.at("constructor_extract_tarball").compute().value<bool>();
            construct(config, prefix, extract_conda_pkgs, extract_tarball);
        }
    );
}

void
construct(Configuration& config, const fs::u8path& prefix, bool extract_conda_pkgs, bool extract_tarball)
{
    config.at("use_target_prefix_fallback").set_value(true);
    config.at("target_prefix_checks")
        .set_value(
            MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX | MAMBA_ALLOW_NOT_ENV_PREFIX
        );
    config.load();

    std::map<std::string, nlohmann::json> repodatas;

    if (extract_conda_pkgs)
    {
        auto find_package = [](nlohmann::json& j, const std::string& fn) -> nlohmann::json
        {
            try
            {
                if (util::ends_with(fn, ".tar.bz2"))
                {
                    return j.at("packages").at(fn);
                }
                else if (util::ends_with(fn, ".conda"))
                {
                    return j.at("packages.conda").at(fn);
                }
            }
            catch (nlohmann::json::out_of_range& /*e*/)
            { /* */
            }
            LOG_WARNING << "Could not find entry in repodata cache for " << fn;
            return {};
        };
        fs::u8path pkgs_dir = prefix / "pkgs";
        fs::u8path urls_file = pkgs_dir / "urls";

        for (const auto& raw_url : read_lines(urls_file))
        {
            auto pkg_info = specs::PackageInfo::from_url(raw_url)
                                .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                .value();

            fs::u8path entry = pkgs_dir / pkg_info.filename;
            LOG_TRACE << "Extracting " << pkg_info.filename << std::endl;
            std::cout << "Extracting " << pkg_info.filename << std::endl;

            fs::u8path base_path = extract(entry, ExtractOptions::from_context(config.context()));

            fs::u8path repodata_record_path = base_path / "info" / "repodata_record.json";
            fs::u8path index_path = base_path / "info" / "index.json";

            std::string channel_url;
            if (pkg_info.package_url.size() > pkg_info.filename.size())
            {
                channel_url = pkg_info.package_url.substr(
                    0,
                    pkg_info.package_url.size() - pkg_info.filename.size()
                );
            }
            std::string repodata_cache_name = util::concat(cache_name_from_url(channel_url), ".json");
            fs::u8path repodata_location = pkgs_dir / "cache" / repodata_cache_name;

            nlohmann::json repodata_record;
            if (fs::exists(repodata_location))
            {
                if (repodatas.find(repodata_cache_name) == repodatas.end())
                {
                    auto infile = open_ifstream(repodata_location);
                    nlohmann::json j;
                    infile >> j;
                    repodatas[repodata_cache_name] = j;
                }
                auto& j = repodatas[repodata_cache_name];
                repodata_record = find_package(j, pkg_info.filename);
            }

            nlohmann::json index;
            std::ifstream index_file{ index_path.std_path() };
            index_file >> index;

            if (!repodata_record.is_null())
            {
                // update values from index if there are any that are not part of the
                // repodata_record.json yet
                repodata_record.insert(index.cbegin(), index.cend());
            }
            else
            {
                LOG_WARNING << "Did not find a repodata record for " << pkg_info.package_url;
                repodata_record = index;

                repodata_record["size"] = fs::file_size(entry);
                if (!pkg_info.md5.empty())
                {
                    repodata_record["md5"] = pkg_info.md5;
                }
                if (!pkg_info.sha256.empty())
                {
                    repodata_record["sha256"] = pkg_info.sha256;
                }
            }

            repodata_record["fn"] = pkg_info.filename;
            repodata_record["url"] = pkg_info.package_url;
            repodata_record["channel"] = pkg_info.channel;

            if (repodata_record.find("size") == repodata_record.end() || repodata_record["size"] == 0)
            {
                repodata_record["size"] = fs::file_size(entry);
            }

            LOG_TRACE << "Writing " << repodata_record_path;
            std::ofstream repodata_record_of{ repodata_record_path.std_path() };
            repodata_record_of << repodata_record.dump(4);
        }
    }

    if (extract_tarball)
    {
        fs::u8path extract_tarball_path = prefix / "_tmp.tar.bz2";
        read_binary_from_stdin_and_write_to_file(extract_tarball_path);
        extract_archive(extract_tarball_path, prefix, ExtractOptions::from_context(config.context()));
        fs::remove(extract_tarball_path);
    }
}

void
read_binary_from_stdin_and_write_to_file(fs::u8path& filename)
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
