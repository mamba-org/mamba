// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/env_lockfile.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/string.hpp"

#include "./env_lockfile_impl.hpp"

namespace mamba
{
    namespace
    {
        auto
        read_env_lockfile_impl(const fs::u8path& lockfile_location, EnvLockfileFormat file_format)
            -> tl::expected<EnvironmentLockFile, mamba_error>
        {
            switch (file_format)
            {
                case EnvLockfileFormat::conda_yaml:
                {
                    return read_conda_environment_lockfile(lockfile_location);
                }
                case EnvLockfileFormat::mambajs_json:
                {
                    return read_mambajs_environment_lockfile(lockfile_location);
                }

                default:
                {
                    return tl::unexpected(
                        EnvLockFileError::make_error(
                            lockfile_parsing_error_code::not_env_lockfile,
                            fmt::format(
                                "file '{}' does not seem to be an environment lockfile or doesn't have a supported format",
                                lockfile_location.string()
                            )
                        )
                    );
                }
            }
        }
    }

    auto read_environment_lockfile(const fs::u8path& lockfile_location, EnvLockfileFormat file_format)
        -> tl::expected<EnvironmentLockFile, mamba_error>
    {
        const auto file_path = fs::absolute(lockfile_location);  // Having the complete path helps
                                                                 // with logging and error reports.

        if (file_format == EnvLockfileFormat::undefined)
        {
            file_format = deduce_env_lockfile_format(lockfile_location);
        }

        return read_env_lockfile_impl(file_path, file_format);
    }

    auto deduce_env_lockfile_format(const mamba::fs::u8path& lockfile_location) -> EnvLockfileFormat
    {
        if (lockfile_location.extension() == ".json")
        {
            return EnvLockfileFormat::mambajs_json;
        }

        const bool is_conda_lockfile = is_conda_env_lockfile_name(
            lockfile_location.filename().string()
        );

        if (is_conda_lockfile == true)
        {
            return EnvLockfileFormat::conda_yaml;
        }

        return EnvLockfileFormat::undefined;
    }

    auto is_conda_env_lockfile_name(std::string_view filename) -> bool
    {
        return filename.ends_with("-lock.yml") || filename.ends_with("-lock.yaml");
    }

    auto is_env_lockfile_name(std::string_view filename) -> bool
    {
        return is_conda_env_lockfile_name(filename) or filename.ends_with(".json");
    }
}
