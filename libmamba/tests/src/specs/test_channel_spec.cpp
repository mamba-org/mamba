// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <doctest/doctest.h>

#include "mamba/specs/channel_spec.hpp"

using namespace mamba;
using namespace mamba::specs;

TEST_SUITE("specs::channel_spec")
{
    TEST_CASE("Parsing")
    {
        using Type = typename ChannelSpec::Type;

        SUBCASE("https://repo.anaconda.com/conda-forge")
        {
            const auto spec = ChannelSpec::parse("https://repo.anaconda.com/conda-forge");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{});
            CHECK_EQ(spec.type(), Type::URL);
        }

        SUBCASE("https://repo.anaconda.com/conda-forge/osx-64")
        {
            const auto spec = ChannelSpec::parse("https://repo.anaconda.com/conda-forge/osx-64");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{ Platform::osx_64 });
            CHECK_EQ(spec.type(), Type::URL);
        }

        SUBCASE("https://repo.anaconda.com/conda-forge[win-64|noarch]")
        {
            const auto spec = ChannelSpec::parse("https://repo.anaconda.com/conda-forge[win-64|noarch]"
            );
            CHECK_EQ(
                spec.platform_filters(),
                util::flat_set<Platform>{ Platform::win_64, Platform::noarch }
            );
            CHECK_EQ(spec.type(), Type::URL);
        }

        SUBCASE("https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda")
        {
            const auto spec = ChannelSpec::parse(
                "https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda"
            );
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{});
            CHECK_EQ(spec.type(), Type::PackageURL);
        }

        SUBCASE("/Users/name/conda")
        {
            const auto spec = ChannelSpec::parse("/Users/name/conda");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{});
            CHECK_EQ(spec.type(), Type::Path);
        }

        SUBCASE("/tmp/pkg-0.0-bld.tar.bz2")
        {
            const auto spec = ChannelSpec::parse("/tmp/pkg-0.0-bld.tar.bz2");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{});
            CHECK_EQ(spec.type(), Type::PackagePath);
        }

        SUBCASE("conda-forge")
        {
            const auto spec = ChannelSpec::parse("conda-forge");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{});
            CHECK_EQ(spec.type(), Type::Name);
        }

        SUBCASE("conda-forge/linux-64")
        {
            const auto spec = ChannelSpec::parse("conda-forge/linux-64");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{ Platform::linux_64 });
            CHECK_EQ(spec.type(), Type::Name);
        }

        SUBCASE("conda-forge[linux-64]")
        {
            const auto spec = ChannelSpec::parse("conda-forge[linux-64]");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{ Platform::linux_64 });
            CHECK_EQ(spec.type(), Type::Name);
        }

        SUBCASE("conda-forge[]")
        {
            const auto spec = ChannelSpec::parse("conda-forge[linux-64]");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{ Platform::linux_64 });
            CHECK_EQ(spec.type(), Type::Name);
        }

        SUBCASE("conda-forge/linux-64/label/foo_dev")
        {
            const auto spec = ChannelSpec::parse("conda-forge/linux-64/label/foo_dev");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{ Platform::linux_64 });
            CHECK_EQ(spec.type(), Type::Name);
        }

        SUBCASE("conda-forge/label/foo_dev[linux-64]")
        {
            const auto spec = ChannelSpec::parse("conda-forge/label/foo_dev[linux-64]");
            CHECK_EQ(spec.platform_filters(), util::flat_set<Platform>{ Platform::linux_64 });
            CHECK_EQ(spec.type(), Type::Name);
        }
    }
}
