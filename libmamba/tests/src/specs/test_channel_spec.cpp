// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/util/build.hpp"
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

        SUBCASE("file:///Users/name/conda")
        {
            const auto spec = ChannelSpec::parse("file:///Users/name/conda");
            CHECK_EQ(spec.type(), Type::Path);
            CHECK_EQ(spec.location(), "file:///Users/name/conda");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("file:///Users/name/conda[linux-64]")
        {
            const auto spec = ChannelSpec::parse("file:///Users/name/conda[linux-64]");
            CHECK_EQ(spec.type(), Type::Path);
            CHECK_EQ(spec.location(), "file:///Users/name/conda");
            CHECK_EQ(spec.platform_filters(), PlatformSet{ "linux-64" });
        }

        SUBCASE("file://C:/Users/name/conda")
        {
            if (util::on_win)
            {
                const auto spec = ChannelSpec::parse("file://C:/Users/name/conda");
                CHECK_EQ(spec.type(), Type::Path);
                CHECK_EQ(spec.location(), "file://C:/Users/name/conda");
                CHECK_EQ(spec.platform_filters(), PlatformSet{});
            }
        }

        SUBCASE("/Users/name/conda")
        {
            const auto spec = ChannelSpec::parse("/Users/name/conda");
            CHECK_EQ(spec.type(), Type::Path);
            CHECK_EQ(spec.location(), "/Users/name/conda");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("./folder/../folder/.")
        {
            const auto spec = ChannelSpec::parse("./folder/../folder/.");
            CHECK_EQ(spec.type(), Type::Path);
            CHECK_EQ(spec.location(), "folder");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("~/folder/")
        {
            const auto spec = ChannelSpec::parse("~/folder/");
            CHECK_EQ(spec.type(), Type::Path);
            CHECK_EQ(spec.location(), "~/folder");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("/tmp/pkg-0.0-bld.tar.bz2")
        {
            const auto spec = ChannelSpec::parse("/tmp/pkg-0.0-bld.tar.bz2");
            CHECK_EQ(spec.type(), Type::PackagePath);
            CHECK_EQ(spec.location(), "/tmp/pkg-0.0-bld.tar.bz2");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("C:/tmp//pkg-0.0-bld.tar.bz2")
        {
            const auto spec = ChannelSpec::parse("C:/tmp//pkg-0.0-bld.tar.bz2");
            CHECK_EQ(spec.type(), Type::PackagePath);
            CHECK_EQ(spec.location(), "C:/tmp/pkg-0.0-bld.tar.bz2");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE(R"(C:\tmp\pkg-0.0-bld.tar.bz2)")
        {
            if (util::on_win)
            {
                const auto spec = ChannelSpec::parse(R"(C:\tmp\pkg-0.0-bld.tar.bz2)");
                CHECK_EQ(spec.type(), Type::PackagePath);
                CHECK_EQ(spec.location(), "C:/tmp/pkg-0.0-bld.tar.bz2");
                CHECK_EQ(spec.platform_filters(), PlatformSet{});
            }
        }

        SUBCASE("conda-forge")
        {
            const auto spec = ChannelSpec::parse("conda-forge");
            CHECK_EQ(spec.type(), Type::Name);
            CHECK_EQ(spec.location(), "conda-forge");
            CHECK_EQ(spec.platform_filters(), PlatformSet{});
        }

        SUBCASE("repo.anaconda.com")
        {
            const auto spec = ChannelSpec::parse("repo.anaconda.com");
            // Unintuitive but correct type, this is not a URL. Better explicit than clever.
            CHECK_EQ(spec.type(), Type::Name);
            CHECK_EQ(spec.location(), "repo.anaconda.com");
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
