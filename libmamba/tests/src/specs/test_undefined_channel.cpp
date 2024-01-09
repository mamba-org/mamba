// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/undefined_channel.hpp"
#include "mamba/util/build.hpp"

using namespace mamba;
using namespace mamba::specs;

TEST_SUITE("specs::channel_spec")
{
    using PlatformSet = typename util::flat_set<std::string>;
    using Type = typename UndefinedChannel::Type;

    TEST_CASE("Constructor")
    {
        SUBCASE("Default")
        {
            const auto uc = UndefinedChannel();
            CHECK_EQ(uc.type(), UndefinedChannel::Type::Unknown);
            CHECK_EQ(uc.location(), "<unknown>");
            CHECK(uc.platform_filters().empty());
        }

        SUBCASE("Unknown")
        {
            const auto uc = UndefinedChannel("hello", { "linux-78" }, UndefinedChannel::Type::Unknown);
            CHECK_EQ(uc.type(), UndefinedChannel::Type::Unknown);
            CHECK_EQ(uc.location(), "<unknown>");
            CHECK_EQ(uc.platform_filters(), PlatformSet{ "linux-78" });
        }
    }

    TEST_CASE("Parsing")
    {
        SUBCASE("Invalid channels")
        {
            for (std::string_view str : { "", "<unknown>", ":///<unknown>", "none" })
            {
                CAPTURE(str);
                const auto uc = UndefinedChannel::parse(str);
                CHECK_EQ(uc.type(), Type::Unknown);
                CHECK_EQ(uc.location(), "<unknown>");
                CHECK_EQ(uc.platform_filters(), PlatformSet{});
            }
        }

        SUBCASE("https://repo.anaconda.com/conda-forge")
        {
            const auto uc = UndefinedChannel::parse("https://repo.anaconda.com/conda-forge");
            CHECK_EQ(uc.type(), Type::URL);
            CHECK_EQ(uc.location(), "https://repo.anaconda.com/conda-forge");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE("https://repo.anaconda.com/conda-forge/osx-64")
        {
            const auto uc = UndefinedChannel::parse("https://repo.anaconda.com/conda-forge/osx-64");
            CHECK_EQ(uc.type(), Type::URL);
            CHECK_EQ(uc.location(), "https://repo.anaconda.com/conda-forge");
            CHECK_EQ(uc.platform_filters(), PlatformSet{ "osx-64" });
        }

        SUBCASE("https://repo.anaconda.com/conda-forge[win-64|noarch]")
        {
            const auto uc = UndefinedChannel::parse(
                "https://repo.anaconda.com/conda-forge[win-64|noarch]"
            );
            CHECK_EQ(uc.type(), Type::URL);
            CHECK_EQ(uc.location(), "https://repo.anaconda.com/conda-forge");
            CHECK_EQ(uc.platform_filters(), PlatformSet{ "win-64", "noarch" });
        }

        SUBCASE("https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda")
        {
            const auto uc = UndefinedChannel::parse(
                "https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda"
            );
            CHECK_EQ(uc.type(), Type::PackageURL);
            CHECK_EQ(uc.location(), "https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE("file:///Users/name/conda")
        {
            const auto uc = UndefinedChannel::parse("file:///Users/name/conda");
            CHECK_EQ(uc.type(), Type::Path);
            CHECK_EQ(uc.location(), "file:///Users/name/conda");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE("file:///Users/name/conda[linux-64]")
        {
            const auto uc = UndefinedChannel::parse("file:///Users/name/conda[linux-64]");
            CHECK_EQ(uc.type(), Type::Path);
            CHECK_EQ(uc.location(), "file:///Users/name/conda");
            CHECK_EQ(uc.platform_filters(), PlatformSet{ "linux-64" });
        }

        SUBCASE("file://C:/Users/name/conda")
        {
            if (util::on_win)
            {
                const auto uc = UndefinedChannel::parse("file://C:/Users/name/conda");
                CHECK_EQ(uc.type(), Type::Path);
                CHECK_EQ(uc.location(), "file://C:/Users/name/conda");
                CHECK_EQ(uc.platform_filters(), PlatformSet{});
            }
        }

        SUBCASE("/Users/name/conda")
        {
            const auto uc = UndefinedChannel::parse("/Users/name/conda");
            CHECK_EQ(uc.type(), Type::Path);
            CHECK_EQ(uc.location(), "/Users/name/conda");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE("./folder/../folder/.")
        {
            const auto uc = UndefinedChannel::parse("./folder/../folder/.");
            CHECK_EQ(uc.type(), Type::Path);
            CHECK_EQ(uc.location(), "folder");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE("~/folder/")
        {
            const auto uc = UndefinedChannel::parse("~/folder/");
            CHECK_EQ(uc.type(), Type::Path);
            CHECK_EQ(uc.location(), "~/folder");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE("/tmp/pkg-0.0-bld.tar.bz2")
        {
            const auto uc = UndefinedChannel::parse("/tmp/pkg-0.0-bld.tar.bz2");
            CHECK_EQ(uc.type(), Type::PackagePath);
            CHECK_EQ(uc.location(), "/tmp/pkg-0.0-bld.tar.bz2");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE("C:/tmp//pkg-0.0-bld.tar.bz2")
        {
            const auto uc = UndefinedChannel::parse("C:/tmp//pkg-0.0-bld.tar.bz2");
            CHECK_EQ(uc.type(), Type::PackagePath);
            CHECK_EQ(uc.location(), "C:/tmp/pkg-0.0-bld.tar.bz2");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE(R"(C:\tmp\pkg-0.0-bld.tar.bz2)")
        {
            if (util::on_win)
            {
                const auto uc = UndefinedChannel::parse(R"(C:\tmp\pkg-0.0-bld.tar.bz2)");
                CHECK_EQ(uc.type(), Type::PackagePath);
                CHECK_EQ(uc.location(), "C:/tmp/pkg-0.0-bld.tar.bz2");
                CHECK_EQ(uc.platform_filters(), PlatformSet{});
            }
        }

        SUBCASE("conda-forge")
        {
            const auto uc = UndefinedChannel::parse("conda-forge");
            CHECK_EQ(uc.type(), Type::Name);
            CHECK_EQ(uc.location(), "conda-forge");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE("repo.anaconda.com")
        {
            const auto uc = UndefinedChannel::parse("repo.anaconda.com");
            // Unintuitive but correct type, this is not a URL. Better explicit than clever.
            CHECK_EQ(uc.type(), Type::Name);
            CHECK_EQ(uc.location(), "repo.anaconda.com");
            CHECK_EQ(uc.platform_filters(), PlatformSet{});
        }

        SUBCASE("conda-forge/linux-64")
        {
            const auto uc = UndefinedChannel::parse("conda-forge/linux-64");
            CHECK_EQ(uc.type(), Type::Name);
            CHECK_EQ(uc.location(), "conda-forge");
            CHECK_EQ(uc.platform_filters(), PlatformSet{ "linux-64" });
        }

        SUBCASE("conda-forge[linux-avx512]")
        {
            const auto uc = UndefinedChannel::parse("conda-forge[linux-avx512]");
            CHECK_EQ(uc.type(), Type::Name);
            CHECK_EQ(uc.location(), "conda-forge");
            CHECK_EQ(uc.platform_filters(), PlatformSet{ "linux-avx512" });
        }

        SUBCASE("conda-forge[]")
        {
            const auto uc = UndefinedChannel::parse("conda-forge[linux-64]");
            CHECK_EQ(uc.type(), Type::Name);
            CHECK_EQ(uc.location(), "conda-forge");
            CHECK_EQ(uc.platform_filters(), PlatformSet{ "linux-64" });
        }

        SUBCASE("conda-forge/linux-64/label/foo_dev")
        {
            const auto uc = UndefinedChannel::parse("conda-forge/linux-64/label/foo_dev");
            CHECK_EQ(uc.type(), Type::Name);
            CHECK_EQ(uc.location(), "conda-forge/label/foo_dev");
            CHECK_EQ(uc.platform_filters(), PlatformSet{ "linux-64" });
        }

        SUBCASE("conda-forge/label/foo_dev[linux-64]")
        {
            const auto uc = UndefinedChannel::parse("conda-forge/label/foo_dev[linux-64]");
            CHECK_EQ(uc.type(), Type::Name);
            CHECK_EQ(uc.location(), "conda-forge/label/foo_dev");
            CHECK_EQ(uc.platform_filters(), PlatformSet{ "linux-64" });
        }
    }

    TEST_CASE("str")
    {
        CHECK_EQ(UndefinedChannel("location", {}, Type::Name).str(), "location");
        CHECK_EQ(
            UndefinedChannel("location", { "linux-64", "noarch" }, Type::Name).str(),
            "location[linux-64,noarch]"
        );
    }
}
