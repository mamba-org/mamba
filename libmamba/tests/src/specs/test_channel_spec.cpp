// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

using namespace mamba;
using namespace mamba::specs;

TEST_SUITE("specs::channel_spec")
{
    TEST_CASE("Parsing")
    {
        using Type = typename ChannelSpec::Type;
        using PlatformSet = typename util::flat_set<std::string>;

        SUBCASE("https://repo.anaconda.com/conda-forge")
        {
            const auto spec = ChannelSpec::parse("https://repo.anaconda.com/conda-forge");
            CHECK_EQ(spec.type(), Type::URL);
            CHECK_EQ(spec.location(), "https://repo.anaconda.com/conda-forge");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("https://repo.anaconda.com/conda-forge/osx-64")
        {
            const auto spec = ChannelSpec::parse("https://repo.anaconda.com/conda-forge/osx-64");
            CHECK_EQ(spec.type(), Type::URL);
            CHECK_EQ(spec.location(), "https://repo.anaconda.com/conda-forge");
            CHECK_EQ(spec.platform_filters(), PlatformSet{ "osx-64" });
        }

        SUBCASE("https://repo.anaconda.com/conda-forge[win-64|noarch]")
        {
            const auto spec = ChannelSpec::parse("https://repo.anaconda.com/conda-forge[win-64|noarch]"
            );
            CHECK_EQ(spec.type(), Type::URL);
            CHECK_EQ(spec.location(), "https://repo.anaconda.com/conda-forge");
            CHECK_EQ(spec.platform_filters(), PlatformSet{ "win-64", "noarch" });
        }

        SUBCASE("https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda")
        {
            const auto spec = ChannelSpec::parse(
                "https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda"
            );
            CHECK_EQ(spec.type(), Type::PackageURL);
            CHECK_EQ(spec.location(), "https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("/Users/name/conda")
        {
            const auto spec = ChannelSpec::parse("/Users/name/conda");
            CHECK_EQ(spec.type(), Type::Path);
            CHECK_EQ(spec.location(), "file:///Users/name/conda");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("./folder/../folder/.")
        {
            const auto expected_folder = fs::weakly_canonical("./folder");
            const auto spec = ChannelSpec::parse("./folder/../folder/.");
            CHECK_EQ(spec.type(), Type::Path);
            CHECK_EQ(spec.location(), util::concat("file://", util::path_to_posix(expected_folder)));
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("/tmp/pkg-0.0-bld.tar.bz2")
        {
            const auto spec = ChannelSpec::parse("/tmp/pkg-0.0-bld.tar.bz2");
            CHECK_EQ(spec.type(), Type::PackagePath);
            CHECK_EQ(spec.location(), "file:///tmp/pkg-0.0-bld.tar.bz2");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("conda-forge")
        {
            const auto spec = ChannelSpec::parse("conda-forge");
            CHECK_EQ(spec.type(), Type::Name);
            CHECK_EQ(spec.location(), "conda-forge");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("conda-forge/linux-64")
        {
            const auto spec = ChannelSpec::parse("conda-forge/linux-64");
            CHECK_EQ(spec.type(), Type::Name);
            CHECK_EQ(spec.location(), "conda-forge");
            CHECK_EQ(spec.platform_filters(), PlatformSet{ "linux-64" });
        }

        SUBCASE("conda-forge[linux-avx512]")
        {
            const auto spec = ChannelSpec::parse("conda-forge[linux-avx512]");
            CHECK_EQ(spec.type(), Type::Name);
            CHECK_EQ(spec.location(), "conda-forge");
            CHECK_EQ(spec.platform_filters(), PlatformSet{ "linux-avx512" });
        }

        SUBCASE("conda-forge[]")
        {
            const auto spec = ChannelSpec::parse("conda-forge[linux-64]");
            CHECK_EQ(spec.type(), Type::Name);
            CHECK_EQ(spec.location(), "conda-forge");
            CHECK_EQ(spec.platform_filters(), PlatformSet{ "linux-64" });
        }

        SUBCASE("conda-forge/linux-64/label/foo_dev")
        {
            const auto spec = ChannelSpec::parse("conda-forge/linux-64/label/foo_dev");
            CHECK_EQ(spec.type(), Type::Name);
            CHECK_EQ(spec.location(), "conda-forge/label/foo_dev");
            CHECK_EQ(spec.platform_filters(), PlatformSet{ "linux-64" });
        }

        SUBCASE("conda-forge/label/foo_dev[linux-64]")
        {
            const auto spec = ChannelSpec::parse("conda-forge/label/foo_dev[linux-64]");
            CHECK_EQ(spec.type(), Type::Name);
            CHECK_EQ(spec.location(), "conda-forge/label/foo_dev");
            CHECK_EQ(spec.platform_filters(), PlatformSet{ "linux-64" });
        }
    }
}
