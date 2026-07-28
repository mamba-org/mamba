// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_VIRTUAL_PACKAGES_HPP
#define MAMBA_CORE_VIRTUAL_PACKAGES_HPP

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "mamba/specs/package_info.hpp"

namespace mamba
{
    std::vector<specs::PackageInfo> get_virtual_packages(
        const std::string& platform,
        const std::map<std::string, std::string>& override_virtual_packages = {}
    );

    namespace detail
    {
        /** Resolve a virtual-package override: `CONDA_OVERRIDE_<NAME>` then config map. */
        [[nodiscard]] auto get_virtual_package_override(
            std::string_view name,
            const std::map<std::string, std::string>& overrides
        ) -> std::optional<std::string>;

        std::string
        cuda_version(const std::map<std::string, std::string>& override_virtual_packages = {});

        auto make_virtual_package(
            std::string name,
            std::string subdir,
            std::string version = "",
            std::string build_string = ""
        ) -> specs::PackageInfo;

        std::vector<specs::PackageInfo> dist_packages(
            const std::string& platform,
            const std::map<std::string, std::string>& override_virtual_packages = {}
        );
    }
}

#endif
