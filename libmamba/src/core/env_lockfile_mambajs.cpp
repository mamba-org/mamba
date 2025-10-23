// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <ranges>

#include <nlohmann/json.hpp>

#include "mamba/core/env_lockfile.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    using json = nlohmann::json;

    namespace env_lockfile_mambajs_v1
    {
        using Package = EnvironmentLockFile::Package;

        auto read_package_info(const json& package_value) -> tl::expected<Package, mamba_error>
        {
            Package package{
                .info = specs::PackageInfo{ package_value["name"].get<std::string>() },
                // clang-format: off
                .is_optional =
                    [&]
                {
                    if (const auto& optional_node = package_node["optional"])
                    {
                        return optional_node.as<bool>();
                    }
                    return false;
                }(),
                // clang-format: on
                .category = package_node["category"] ? package_node["category"].as<std::string>()
                                                     : "main",
                .manager = package_node["manager"].as<std::string>(),
                .platform = package_node["platform"].as<std::string>(),
            };

            package.info.version = package_node["version"].as<std::string>();
            const auto& hash_node = package_node["hash"];

            if (const auto& md5_node = hash_node["md5"])
            {
                package.info.md5 = md5_node.as<std::string>();
            }

            if (const auto& sha256_node = hash_node["sha256"])
            {
                package.info.sha256 = sha256_node.as<std::string>();
            }

            if (package.info.sha256.empty() && package.info.md5.empty())
            {
                return tl::unexpected(EnvLockFileError::make_error(
                    file_parsing_error_code::invalid_data,
                    "either package 'package.info.hash.md5' or 'package.info.hash.sha256' must be specified, found none"
                ));
            }

            package.info.package_url = package_node["url"].as<std::string_view>();
            {
                auto maybe_parsed_info = specs::PackageInfo::from_url(package.info.package_url);
                if (!maybe_parsed_info)
                {
                    return make_unexpected(
                        maybe_parsed_info.error().what(),
                        mamba_error_code::invalid_spec
                    );
                }
                package.info.filename = maybe_parsed_info->filename;
                package.info.channel = maybe_parsed_info->channel;
                package.info.build_string = maybe_parsed_info->build_string;
                package.info.platform = maybe_parsed_info->platform;
            }

            for (const auto& dependency : package_node["dependencies"])
            {
                const auto dependency_name = dependency.first.as<std::string>();
                const auto dependency_constraint = dependency.second.as<std::string>();
                package.info.dependencies.push_back(
                    fmt::format("{} {}", dependency_name, dependency_constraint)
                );
            }

            if (const auto& constraints_nodes = package_node["constrains"])
            {
                for (const auto& dependency : constraints_nodes)
                {
                    const auto constraint_dep_name = dependency.first.as<std::string>();
                    const auto constraint_expr = dependency.second.as<std::string>();
                    package.info.constrains.push_back(
                        fmt::format("{} {}", constraint_dep_name, constraint_expr)
                    );
                }
            }

            return package;
        }

        auto read_metadata(const json& metadata_value)
            -> tl::expected<EnvironmentLockFile::Meta, mamba_error>
        {
            EnvironmentLockFile::Meta metadata;

            metadata.platforms.push_back(metadata_value["platform"].get<std::string>());

            if (metadata.platforms.front().empty())
            {
                return tl::unexpected(EnvLockFileError::make_error(
                    file_parsing_error_code::invalid_data,
                    "a `platform` must be specified, found empty value"
                ));
            }

            for (const auto& channel_name : metadata_value["channels"])
            {
                EnvironmentLockFile::Channel channel;
                auto channel_info = metadata_value["channelInfo"][channel_name.get<std::string>()];
                channel.url = channel_info["url"].get<std::string>();
                metadata.channels.push_back(std::move(channel));
            }

            // content hash is not currently part of the spec
            for (const auto& [key, value] : metadata_value["content_hash"].items())
            {
                metadata.content_hash.emplace(key, value);
            }


            return metadata;
        }

        auto read_environment_lockfile(const json& lockfile_value)
            -> tl::expected<EnvironmentLockFile, mamba_error>
        {
            const auto& maybe_metadata = read_metadata(lockfile_value);
            if (!maybe_metadata)
            {
                return tl::unexpected(maybe_metadata.error());
            }

            auto metadata = maybe_metadata.value();

            // TODO: python packages too
            std::vector<Package> packages;
            for (const auto& package_value : lockfile_value["packages"])
            {
                if (auto maybe_package = read_package_info(package_value))
                {
                    packages.push_back(maybe_package.value());
                }
                else
                {
                    return tl::unexpected(maybe_package.error());
                }
            }

            return EnvironmentLockFile{ std::move(metadata), std::move(packages) };
        }
    }

    namespace
    {
        auto read_json_file(const fs::u8path& file_location)
            -> tl::expected<json, std::string>
        {
            // Here we use a technique letting us obtaining a readable system message if any error
            // happens when opening the file.
            // TODO: generalize this technique for other cases where we open files and dont
            // have a reason for failure (util::open... etc.?)
            std::ifstream lockfile;
            lockfile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

            try
            {
                lockfile.open(file_location.std_path());
            }
            catch (const std::system_error& ferr)
            {
                return tl::unexpected(ferr.what());
            }

            return json::parse(lockfile);  // Let this parsing throw for the caller.
        }
    }

    auto read_mambajs_environment_lockfile(const fs::u8path& lockfile_location)
        -> tl::expected<EnvironmentLockFile, mamba_error>
    {
        assert(lockfile_location.is_absolute());

        // see:
        // - v1.0.0 :
        // https://github.com/emscripten-forge/mambajs/blob/main/packages/mambajs-core/schema/lock.v1.0.0.json
        // - v1.0.1 :
        // https://github.com/emscripten-forge/mambajs/blob/main/packages/mambajs-core/schema/lock.v1.0.1.json

        try
        {
            // TODO: add fields validation here (using some schema validation tool)

            const auto maybe_lockfile_content = read_json_file(lockfile_location);
            if (not maybe_lockfile_content)
            {
                return tl::unexpected(EnvLockFileError::make_error(
                    file_parsing_error_code::parsing_failure,
                    fmt::format(
                        "failed to open environment lockfile located at '{}': {}",
                        lockfile_location.string(),
                        maybe_lockfile_content.error()
                    )
                ));
            }

            const auto& lockfile_content = *maybe_lockfile_content;

            const auto lockfile_version = lockfile_content["lockVersion"].get<std::string>();
            if (lockfile_version.starts_with("1.0."))
            {
                return env_lockfile_mambajs_v1::read_environment_lockfile(lockfile_content);
            }
            else
            {
                return tl::unexpected(EnvLockFileError::make_error(
                    file_parsing_error_code::unsupported_version,
                    fmt::format(
                        "Failed to read environment lockfile at '{}' : unknown version '{}'",
                        lockfile_location.string(),
                        lockfile_version
                    )
                ));
            }
        }
        catch (const json::parse_error& err)
        {
            return tl::unexpected(EnvLockFileError::make_error(
                file_parsing_error_code::parsing_failure,
                fmt::format(
                    "JSON parsing error while reading environment lockfile located at '{}', byte {} : {}",
                    lockfile_location.string(),
                    err.byte,
                    err.what()
                ),
                std::type_index{ typeid(err) }
            ));
        }
        catch (const std::exception& e)
        {
            return tl::unexpected(EnvLockFileError::make_error(
                file_parsing_error_code::parsing_failure,
                fmt::format(
                    "Error while reading environment lockfile located at '{}': {}",
                    lockfile_location.string(),
                    e.what()
                )
            ));
        }
    }

}
