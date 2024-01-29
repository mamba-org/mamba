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

    std::vector<specs::PackageInfo> get_virtual_packages(const Context& context);

    namespace detail
    {
        std::string cuda_version();
        std::string get_arch();

        specs::PackageInfo make_virtual_package(
            const std::string& name,
            const std::string& subdir,
            const std::string& version = "",
            const std::string& build_string = ""
        );

        std::vector<specs::PackageInfo> dist_packages(const Context& context);
    }
}

#endif
