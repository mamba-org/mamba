// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/core.h>
#include <yaml-cpp/yaml.h>

#include "mamba/core/env_lockfile.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace env_lockfile_conda_v1
    {
        using Package = EnvironmentLockFile::Package;

        tl::expected<Package, mamba_error> read_package_info(const YAML::Node& package_node)
        {
            Package package{
                .info = specs::PackageInfo{ package_node["name"].as<std::string>() },
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
                    lockfile_parsing_error_code::invalid_data,
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

        auto read_metadata(const YAML::Node& metadata_node)
            -> tl::expected<EnvironmentLockFile::Meta, mamba_error>
        {
            EnvironmentLockFile::Meta metadata;

            for (const auto& platform_node : metadata_node["platforms"])
            {
                metadata.platforms.push_back(platform_node.as<std::string>());
            }
            if (metadata.platforms.empty())
            {
                return tl::unexpected(EnvLockFileError::make_error(
                    lockfile_parsing_error_code::invalid_data,
                    "at least one 'metadata.platform.*' must be specified, found none"
                ));
            }

            for (const auto& source_node : metadata_node["sources"])
            {
                metadata.sources.push_back(source_node.as<std::string>());
            }
            if (metadata.sources.empty())
            {
                return tl::unexpected(EnvLockFileError::make_error(
                    lockfile_parsing_error_code::invalid_data,
                    "at least one 'metadata.source.*' must be specified, found none"
                ));
            }

            for (const auto& channel_node : metadata_node["channels"])
            {
                EnvironmentLockFile::Channel channel;
                // FIXME: how to get the name?
                channel.urls.push_back(channel_node["url"].as<std::string>());
                channel.used_env_vars = channel_node["used_env_vars"].as<std::vector<std::string>>();
                metadata.channels.push_back(std::move(channel));
            }

            for (const auto& node_pair : metadata_node["content_hash"])
            {
                const auto& platform_node = node_pair.first;
                const auto& hash_node = node_pair.second;
                metadata.content_hash.emplace(
                    platform_node.as<std::string>(),
                    hash_node.as<std::string>()
                );
            }
            if (metadata.content_hash.empty())
            {
                return tl::unexpected(EnvLockFileError::make_error(
                    lockfile_parsing_error_code::invalid_data,
                    "at least one 'metadata.content_hash.*' value must be specified, found none"
                ));
            }

            return metadata;
        }

        auto read_environment_lockfile(const YAML::Node& lockfile_yaml)
            -> tl::expected<EnvironmentLockFile, mamba_error>
        {
            const auto& maybe_metadata = read_metadata(lockfile_yaml["metadata"]);
            if (!maybe_metadata)
            {
                return tl::unexpected(maybe_metadata.error());
            }

            auto metadata = maybe_metadata.value();

            std::vector<Package> packages;
            for (const auto& package_node : lockfile_yaml["package"])
            {
                if (auto maybe_package = read_package_info(package_node))
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

    auto read_conda_environment_lockfile(const fs::u8path& lockfile_location)
        -> tl::expected<EnvironmentLockFile, mamba_error>
    {
        assert(lockfile_location.is_absolute());
        try
        {
            // TODO: add fields validation here (using some schema validation tool)
            const YAML::Node lockfile_content = YAML::LoadFile(lockfile_location.string());
            const auto lockfile_version = lockfile_content["version"].as<int>();
            switch (lockfile_version)
            {
                case 1:
                    return env_lockfile_conda_v1::read_environment_lockfile(lockfile_content);

                default:
                {
                    return tl::unexpected(EnvLockFileError::make_error(
                        lockfile_parsing_error_code::unsupported_version,
                        fmt::format(
                            "Failed to read environment lockfile at '{}' : unknown version '{}'",
                            lockfile_location.string(),
                            lockfile_version
                        )
                    ));
                }
            }
        }
        catch (const YAML::Exception& err)
        {
            return tl::unexpected(EnvLockFileError::make_error(
                lockfile_parsing_error_code::parsing_failure,
                fmt::format(
                    "YAML parsing error while reading environment lockfile located at '{}' : {}",
                    lockfile_location.string(),
                    err.what()
                ),
                std::type_index{ typeid(err) }
            ));
        }
        catch (const std::exception& e)
        {
            return tl::unexpected(EnvLockFileError::make_error(
                lockfile_parsing_error_code::parsing_failure,
                fmt::format(
                    "Error while reading environment lockfile located at '{}': {}",
                    lockfile_location.string(),
                    e.what()
                )
            ));
        }
    }

}
