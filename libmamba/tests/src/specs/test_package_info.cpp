// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/package_info.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::package_info")
{
    TEST_CASE("PackageInfo::from_url")
    {
        SUBCASE("https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda")
        {
            static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda";

            auto pkg = PackageInfo::from_url(url);

            CHECK_EQ(pkg.name, "pkg");
            CHECK_EQ(pkg.version, "6.4");
            CHECK_EQ(pkg.build_string, "bld");
            CHECK_EQ(pkg.filename, "pkg-6.4-bld.conda");
            CHECK_EQ(pkg.package_url, url);
            CHECK_EQ(pkg.md5, "");
        }

        SUBCASE("https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda#7dbaa197d7ba6032caf7ae7f32c1efa0"
        )
        {
            static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda#7dbaa197d7ba6032caf7ae7f32c1efa0";

            auto pkg = PackageInfo::from_url(url);

            CHECK_EQ(pkg.name, "pkg");
            CHECK_EQ(pkg.version, "6.4");
            CHECK_EQ(pkg.build_string, "bld");
            CHECK_EQ(pkg.filename, "pkg-6.4-bld.conda");
            CHECK_EQ(pkg.package_url, url.substr(0, url.rfind('#')));
            CHECK_EQ(pkg.md5, url.substr(url.rfind('#') + 1));
        }
    }
}
