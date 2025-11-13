// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <fstream>
#include <ranges>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include "mamba/core/env_lockfile.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    using json = nlohmann::json;

    namespace
    {
        template <std::ranges::input_range R, typename T, typename Projection = std::identity>
        auto contains(const R& values, const T& value_to_find, Projection projection = {}) -> bool
        {
            using std::end;
            using std::ranges::find;
            return find(values, value_to_find, projection) != end(values);
        }

    }

    namespace env_lockfile_mambajs_v1
    {
        using Package = EnvironmentLockFile::Package;

        auto read_package_info(
            const std::string_view file_name,
            const json& package_value,
            const std::string_view manager
        ) -> tl::expected<Package, mamba_error>
        {
            // Note: pip packages do not provide a platform name
            // FIXME: magic constants
            auto platform = manager != "pip" ? package_value["subdir"].get<std::string>()
                                             : std::string("");

            Package package{
                .info = specs::PackageInfo{ package_value["name"].get<std::string>() },
                .is_optional = false,
                .category = "main",
                .manager = std::string{ manager },
                .platform = platform,
            };

            package.info.version = package_value["version"].get<std::string>();
            if (package_value.contains("hash"))
            {
                const auto& hash_value = package_value["hash"];
                if (hash_value.contains("md5"))
                {
                    package.info.md5 = hash_value["md5"].get<std::string>();
                }

                if (hash_value.contains("sha256"))
                {
                    package.info.sha256 = hash_value["sha256"].get<std::string>();
                }

                if (package.info.sha256.empty() && package.info.md5.empty())
                {
                    return tl::unexpected(
                        EnvLockFileError::make_error(
                            lockfile_parsing_error_code::invalid_data,
                            "'package.hash' provided but neither 'package.hash.md5' nor 'package.hash.sha256' was found, at least one of them must be provided"
                        )
                    );
                }
            }


            package.info.filename = file_name;
            if (manager == "pip")
            {
                package.info.package_url = package_value["url"].get<std::string>();
                package.info.channel = package_value["registry"].get<std::string>();
            }
            else
            {
                package.info.channel = package_value["channel"].get<std::string>();
                package.info.platform = platform;
                package.info.build_string = package_value["build"].get<std::string>();
                // package.info.package_url = ???; The actual url will be deduced later with the
                // chosen channel mirror url
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
                return tl::unexpected(
                    EnvLockFileError::make_error(
                        lockfile_parsing_error_code::invalid_data,
                        "a `platform` must be specified, found empty value"
                    )
                );
            }

            auto channel_names = metadata_value["channels"].get<std::vector<std::string>>();

            for (const auto& [channel_name, channel_specs] : metadata_value["channelInfo"].items())
            {
                EnvironmentLockFile::Channel channel;
                channel.name = channel_name;

                if (not contains(channel_names, channel_name))
                {
                    return tl::unexpected(
                        EnvLockFileError::make_error(
                            lockfile_parsing_error_code::invalid_data,
                            fmt::format(
                                "channel '{}' in 'channelInfo' not found in 'channels' list",
                                channel_name
                            )
                        )
                    );
                }

                for (const auto& channel_spec : channel_specs)
                {
                    channel.urls.push_back(channel_spec["url"].get<std::string>());
                }

                metadata.channels.push_back(std::move(channel));
            }

            for (const auto& channel_name : channel_names)
            {
                if (not contains(metadata.channels, channel_name, &EnvironmentLockFile::Channel::name))
                {
                    return tl::unexpected(
                        EnvLockFileError::make_error(
                            lockfile_parsing_error_code::invalid_data,
                            fmt::format(
                                "channel '{}' in 'channels' list not found in 'channelInfo' list",
                                channel_name
                            )
                        )
                    );
                }
            }

            // content hash is not currently part of the spec, but might be soon
            if (metadata_value.contains("content_hash"))
            {
                for (const auto& [key, value] : metadata_value["content_hash"].items())
                {
                    metadata.content_hash.emplace(key, value.get<std::string>());
                }
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

            std::vector<Package> packages;

            const auto read_packages = [&](std::string_view manager_name, const json& package_list)
            {
                for (const auto& [package_filename, package_value] : package_list.items())
                {
                    if (auto maybe_package = read_package_info(package_filename, package_value, manager_name))
                    {
                        packages.push_back(maybe_package.value());
                    }
                    else
                    {
                        throw maybe_package.error();
                    }
                }
            };

            try
            {
                read_packages("conda", lockfile_value["packages"]);
                read_packages("pip", lockfile_value["pipPackages"]);
            }
            catch (const mamba_error& error)
            {
                return tl::unexpected(error);
            }


            return EnvironmentLockFile{ std::move(metadata), std::move(packages) };
        }
    }

    namespace
    {
        auto read_json_file(const fs::u8path& file_location) -> tl::expected<json, std::string>
        {
            if (not fs::exists(file_location))
            {
                return tl::unexpected(fmt::format("file does exist : {}", file_location.string()));
            }

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
                return tl::unexpected(
                    EnvLockFileError::make_error(
                        lockfile_parsing_error_code::parsing_failure,
                        fmt::format(
                            "failed to open environment lockfile located at '{}': {}",
                            lockfile_location.string(),
                            maybe_lockfile_content.error()
                        )
                    )
                );
            }

            const auto& lockfile_content = *maybe_lockfile_content;

            const auto lockfile_version = lockfile_content["lockVersion"].get<std::string>();
            if (lockfile_version.starts_with("1.0."))
            {
                return env_lockfile_mambajs_v1::read_environment_lockfile(lockfile_content);
            }
            else
            {
                return tl::unexpected(
                    EnvLockFileError::make_error(
                        lockfile_parsing_error_code::unsupported_version,
                        fmt::format(
                            "Failed to read environment lockfile at '{}' : unknown version '{}'",
                            lockfile_location.string(),
                            lockfile_version
                        )
                    )
                );
            }
        }
        catch (const json::parse_error& err)
        {
            return tl::unexpected(
                EnvLockFileError::make_error(
                    lockfile_parsing_error_code::parsing_failure,
                    fmt::format(
                        "JSON parsing error while reading environment lockfile located at '{}', byte {} : {}",
                        lockfile_location.string(),
                        err.byte,
                        err.what()
                    ),
                    std::type_index{ typeid(err) }
                )
            );
        }
        catch (const json::type_error& err)
        {
            return tl::unexpected(
                EnvLockFileError::make_error(
                    lockfile_parsing_error_code::parsing_failure,
                    fmt::format(
                        "JSON value type doesnt match expected type, while reading environment lockfile located at '{}': {}",
                        lockfile_location.string(),
                        err.what()
                    ),
                    std::type_index{ typeid(err) }
                )
            );
        }
        catch (const std::exception& e)
        {
            return tl::unexpected(
                EnvLockFileError::make_error(
                    lockfile_parsing_error_code::parsing_failure,
                    fmt::format(
                        "Error while reading environment lockfile located at '{}': {}",
                        lockfile_location.string(),
                        e.what()
                    )
                )
            );
        }
    }

}
