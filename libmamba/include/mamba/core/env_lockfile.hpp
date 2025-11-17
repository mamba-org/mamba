// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#ifndef MAMBA_CORE_ENVIRONMENT_LOCKFILE_HPP
#define MAMBA_CORE_ENVIRONMENT_LOCKFILE_HPP

#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>

#include <tl/expected.hpp>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba
{
    class Context;
    class ChannelContext;

    enum class lockfile_parsing_error_code
    {
        unknown_failure,      ///< Something failed while parsing but we can't identify what.
        unsupported_version,  ///< The version of the file does not matched supported ver.
        parsing_failure,  ///< The content of the file doesn't match the expected format/language
                          ///< structure or constraints.
        invalid_data,     ///< The structure of the data in the file is fine but some fields have
                          ///< invalid values for our purpose.
        not_env_lockfile  ///< The file doesn't seem to be a valid or supported lockfile file
                          ///< format.
    };

    struct EnvLockFileError  // TODO: inherit from mamba error
    {
        lockfile_parsing_error_code parsing_error_code = lockfile_parsing_error_code::unknown_failure;
        std::optional<std::type_index> error_type{};

        static auto get_details(const mamba_error& error) -> const EnvLockFileError&
        {
            return std::any_cast<const EnvLockFileError&>(error.data());
        }

        template <typename StringT>
        static auto make_error(
            lockfile_parsing_error_code error_code,
            StringT&& msg,
            std::optional<std::type_index> error_type = std::nullopt
        ) -> mamba_error
        {
            return mamba_error{ std::forward<StringT>(msg),
                                mamba_error_code::env_lockfile_parsing_failed,
                                EnvLockFileError{ error_code, error_type } };
        }
    };

    class EnvironmentLockFile
    {
    public:

        struct Channel
        {
            std::string name;
            std::vector<std::string> urls;
            std::vector<std::string> used_env_vars;
        };

        struct Meta
        {
            std::unordered_map<std::string, std::string> content_hash;
            std::vector<Channel> channels;
            std::vector<std::string> platforms;
            std::vector<std::string> sources;
        };

        struct Package
        {
            specs::PackageInfo info;
            bool is_optional = false;
            std::string category;
            std::string manager;
            std::string platform;
        };

        EnvironmentLockFile(Meta metadata, std::vector<Package> packages)
            : m_metadata(std::move(metadata))
            , m_packages(std::move(packages))
        {
        }

        struct PackageFilter
        {
            std::optional<std::string> category = std::nullopt;
            std::optional<std::string> platform = std::nullopt;
            std::optional<std::string> manager = std::nullopt;
            bool allow_no_platform = false;  // will match empty platform

            auto matches(const Package& package) const -> bool
            {
                bool matches_platform = not platform.has_value() or (package.platform == *platform)
                                        or (package.platform == "noarch");
                if (platform.has_value() and not matches_platform and allow_no_platform)
                {
                    matches_platform = package.platform.empty();
                }

                return matches_platform
                       and (not category.has_value() or (package.category == *category))
                       and (not manager.has_value() or (package.manager == *manager));
            }

            auto operator()(const Package& package) const -> bool
            {
                return matches(package);
            }
        };

        template <typename F>
            requires std::is_invocable_r_v<bool, F, const Package&>
        auto get_packages_for(PackageFilter filter, F predicate) const
            -> std::vector<specs::PackageInfo>
        {
            std::vector<specs::PackageInfo> results;

            for (const auto& package : m_packages)
            {
                if (filter.matches(package) and predicate(package))
                {
                    results.push_back(package.info);
                }
            }

            return results;
        }

        auto get_packages_for(PackageFilter filter) const -> std::vector<specs::PackageInfo>
        {
            return get_packages_for(std::move(filter), [](const auto&) { return true; });
        }

        auto get_all_packages() const -> const std::vector<Package>&
        {
            return m_packages;
        }

        auto get_metadata() const -> const Meta&
        {
            return m_metadata;
        }

    private:

        Meta m_metadata;
        std::vector<Package> m_packages;
    };

    /// Describes a format of environment lockfile file supported by this library.
    enum class EnvLockfileFormat
    {
        undefined,    ///< We don't know the format of the file.
        conda_yaml,   ///< conda's yaml-based environment lockfile format.
        mambajs_json  ///< mambajs's json-based environment lockfile format.
    };

    /// Read an environment lock-file and returns it's structured content or an error if
    /// failed.
    ///
    /// @param lockfile_location The filesystem path to the file to open and read.
    /// @param file_format The expected file format of the file. If `undefined`, which is the
    ///        default value, we guess based on the file's extension and content.
    auto read_environment_lockfile(
        const fs::u8path& lockfile_location,
        EnvLockfileFormat file_format = EnvLockfileFormat::undefined
    ) -> tl::expected<EnvironmentLockFile, mamba_error>;


    /// Returns `true` if the filename matches names of files which should be interpreted as conda
    /// or mambajs environment lockfile.
    /// NOTE: this does not check if the file exists.
    auto is_env_lockfile_name(std::string_view filename) -> bool;

    /// Returns `true` if the filename matches names of files which should be interpreted as conda
    /// NOTE: this does not check if the file exists.
    auto is_conda_env_lockfile_name(std::string_view filename) -> bool;

    /// Deduce the environment lockfile format of a file path based on it's filename.
    /// TODO: more info?
    auto deduce_env_lockfile_format(const fs::u8path& lockfile_location) -> EnvLockfileFormat;
}
#endif
