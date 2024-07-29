// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_VIRTUAL_PACKAGES_HPP
#define MAMBA_CORE_VIRTUAL_PACKAGES_HPP

#include <string>
#include <vector>

#include "mamba/specs/package_info.hpp"

namespace mamba
{
    class Context;

    std::vector<specs::PackageInfo> get_virtual_packages(const std::string& platform);

    namespace detail
    {
        std::string cuda_version();

        auto make_virtual_package(
            std::string name,
            std::string subdir,
            std::string version = "",
            std::string build_string = ""
        ) -> specs::PackageInfo;

        std::vector<specs::PackageInfo> dist_packages(const std::string& platform);
    }
}

#endif
