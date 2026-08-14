// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_VIRTUAL_PACKAGES_HPP
#define MAMBA_CORE_VIRTUAL_PACKAGES_HPP

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mamba/specs/package_info.hpp"

namespace mamba
{
    /** Hasher enabling `string_view` lookup in `override_virtual_packages_map`. */
    struct override_virtual_package_hash
    {
        using is_transparent = void;

        [[nodiscard]] auto operator()(std::string_view sv) const noexcept -> std::size_t
        {
            return std::hash<std::string_view>{}(sv);
        }
    };

    using override_virtual_packages_map = std::
        unordered_map<std::string, std::string, override_virtual_package_hash, std::equal_to<>>;

    std::vector<specs::PackageInfo> get_virtual_packages(
        const std::string& platform,
        const override_virtual_packages_map& override_virtual_packages = {}
    );

    namespace detail
    {
        /** Resolve a virtual-package override: `CONDA_OVERRIDE_<NAME>` then config map. */
        [[nodiscard]] auto
        get_virtual_package_override(std::string_view name, const override_virtual_packages_map& overrides)
            -> std::optional<std::string>;

        std::string cuda_version(const override_virtual_packages_map& override_virtual_packages = {});

        auto make_virtual_package(
            std::string name,
            std::string subdir,
            std::string version = "",
            std::string build_string = ""
        ) -> specs::PackageInfo;

        std::vector<specs::PackageInfo> dist_packages(
            const std::string& platform,
            const override_virtual_packages_map& override_virtual_packages = {}
        );
    }
}

#endif
