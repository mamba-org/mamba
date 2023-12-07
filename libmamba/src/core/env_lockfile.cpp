// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace env_lockfile_v1
    {
        using Package = EnvironmentLockFile::Package;

        tl::expected<Package, mamba_error>
        read_package_info(const Context& ctx, ChannelContext& channel_context, const YAML::Node& package_node)
        {
            Package package{
                /* .info = */ mamba::PackageInfo{ package_node["name"].as<std::string>() },
                /* .is_optional = */
                [&]
                {
                    if (const auto& optional_node = package_node["optional"])
                    {
                        return optional_node.as<bool>();
                    }
                    return false;
                }(),
                /* .category = */
                package_node["category"] ? package_node["category"].as<std::string>() : "main",
                /* .manager = */ package_node["manager"].as<std::string>(),
                /* .platform = */ package_node["platform"].as<std::string>(),
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

            package.info.url = package_node["url"].as<std::string>();
            const MatchSpec spec{ package.info.url };
            package.info.fn = spec.fn;
            package.info.build_string = spec.build_string;
            if (spec.channel.has_value())
            {
                package.info.channel = spec.channel->location();
                if (!spec.channel->platform_filters().empty())
                {
                    // There must be only one since we are expecting URLs
                    assert(spec.channel->platform_filters().size() == 1);
                    package.info.subdir = spec.channel->platform_filters().front();
                }
            }

            for (const auto& dependency : package_node["dependencies"])
            {
                const auto dependency_name = dependency.first.as<std::string>();
                const auto dependency_constraint = dependency.second.as<std::string>();
                package.info.depends.push_back(
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

        tl::expected<EnvironmentLockFile::Meta, mamba_error>
        read_metadata(const YAML::Node& metadata_node)
        {
            EnvironmentLockFile::Meta metadata;

            for (const auto& platform_node : metadata_node["platforms"])
            {
                metadata.platforms.push_back(platform_node.as<std::string>());
            }
            if (metadata.platforms.empty())
            {
                return tl::unexpected(EnvLockFileError::make_error(
                    file_parsing_error_code::invalid_data,
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
                    file_parsing_error_code::invalid_data,
                    "at least one 'metadata.source.*' must be specified, found none"
                ));
            }

            for (const auto& channel_node : metadata_node["channels"])
            {
                EnvironmentLockFile::Channel channel;
                channel.url = channel_node["url"].as<std::string>();
                channel.used_env_vars = channel_node["used_env_vars"].as<std::vector<std::string>>();
                metadata.channels.push_back(std::move(channel));
            }

            for (const auto& node_pair : metadata_node["content_hash"])
            {
                const auto& platform_node = node_pair.first;
                const auto& hash_node = node_pair.first;
                metadata.content_hash.emplace(
                    platform_node.as<std::string>(),
                    hash_node.as<std::string>()
                );
            }
            if (metadata.content_hash.empty())
            {
                return tl::unexpected(EnvLockFileError::make_error(
                    file_parsing_error_code::invalid_data,
                    "at least one 'metadata.content_hash.*' value must be specified, found none"
                ));
            }

            return metadata;
        }

        tl::expected<EnvironmentLockFile, mamba_error> read_environment_lockfile(
            const Context& ctx,
            ChannelContext& channel_context,
            const YAML::Node& lockfile_yaml
        )
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
                if (auto maybe_package = read_package_info(ctx, channel_context, package_node))
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

    tl::expected<EnvironmentLockFile, mamba_error> read_environment_lockfile(
        const Context& ctx,
        ChannelContext& channel_context,
        const fs::u8path& lockfile_location
    )
    {
        const auto file_path = fs::absolute(lockfile_location);  // Having the complete path helps
                                                                 // with logging and error reports.
        try
        {
            // TODO: add fields validation here (using some schema validation tool)
            const YAML::Node lockfile_content = YAML::LoadFile(file_path.string());
            const auto lockfile_version = lockfile_content["version"].as<int>();
            switch (lockfile_version)
            {
                case 1:
                    return env_lockfile_v1::read_environment_lockfile(
                        ctx,
                        channel_context,
                        lockfile_content
                    );

                default:
                {
                    return tl::unexpected(EnvLockFileError::make_error(
                        file_parsing_error_code::unsuported_version,
                        fmt::format(
                            "Failed to read environment lockfile at '{}' : unknown version '{}'",
                            file_path.string(),
                            lockfile_version
                        )
                    ));
                }
            }
        }
        catch (const YAML::Exception& err)
        {
            return tl::unexpected(EnvLockFileError::make_error(
                file_parsing_error_code::parsing_failure,
                fmt::format(
                    "YAML parsing error while reading environment lockfile located at '{}' : {}",
                    file_path.string(),
                    err.what()
                ),
                std::type_index{ typeid(err) }
            ));
        }
        catch (...)
        {
            return tl::unexpected(EnvLockFileError::make_error(
                file_parsing_error_code::parsing_failure,
                fmt::format(
                    "unknown error while reading environment lockfile located at '{}'",
                    file_path.string()
                )
            ));
        }
    }

    std::vector<PackageInfo> EnvironmentLockFile::get_packages_for(
        std::string_view category,
        std::string_view platform,
        std::string_view manager
    ) const
    {
        std::vector<PackageInfo> results;

        // TODO: c++20 - rewrite this with ranges
        const auto package_predicate = [&](const auto& package)
        {
            return package.platform == platform && package.category == category
                   && package.manager == manager;
        };
        for (const auto& package : m_packages)
        {
            if (package_predicate(package))
            {
                results.push_back(package.info);
            }
        }

        return results;
    }

    bool is_env_lockfile_name(std::string_view filename)
    {
        return util::ends_with(filename, "-lock.yml") || util::ends_with(filename, "-lock.yaml");
    }
}
