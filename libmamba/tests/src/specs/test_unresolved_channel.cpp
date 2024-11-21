// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/util/build.hpp"

using namespace mamba;
using namespace mamba::specs;

namespace
{
    using PlatformSet = typename util::flat_set<std::string>;
    using Type = typename UnresolvedChannel::Type;

    TEST_CASE("Constructor")
    {
        SECTION("Default")
        {
            const auto uc = UnresolvedChannel();
            REQUIRE(uc.type() == UnresolvedChannel::Type::Unknown);
            REQUIRE(uc.location() == "<unknown>");
            REQUIRE(uc.platform_filters().empty());
        }

        SECTION("Unknown")
        {
            const auto uc = UnresolvedChannel("hello", { "linux-78" }, UnresolvedChannel::Type::Unknown);
            REQUIRE(uc.type() == UnresolvedChannel::Type::Unknown);
            REQUIRE(uc.location() == "<unknown>");
            REQUIRE(uc.platform_filters() == PlatformSet{ "linux-78" });
        }
    }

    TEST_CASE("Parsing")
    {
        SECTION("Unknown channels")
        {
            for (std::string_view str : { "", "<unknown>", ":///<unknown>", "none" })
            {
                CAPTURE(str);
                const auto uc = UnresolvedChannel::parse(str).value();
                REQUIRE(uc.type() == Type::Unknown);
                REQUIRE(uc.location() == "<unknown>");
                REQUIRE(uc.platform_filters() == PlatformSet{});
            }
        }

        SECTION("Invalid channels")
        {
            for (std::string_view str : { "forgelinux-64]" })
            {
                CAPTURE(str);
                REQUIRE_FALSE(UnresolvedChannel::parse(str).has_value());
            }
        }

        SECTION("https://repo.anaconda.com/conda-forge")
        {
            const auto uc = UnresolvedChannel::parse("https://repo.anaconda.com/conda-forge").value();
            REQUIRE(uc.type() == Type::URL);
            REQUIRE(uc.location() == "https://repo.anaconda.com/conda-forge");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("https://repo.anaconda.com/conda-forge/osx-64")
        {
            const auto uc = UnresolvedChannel::parse("https://repo.anaconda.com/conda-forge/osx-64")
                                .value();
            REQUIRE(uc.type() == Type::URL);
            REQUIRE(uc.location() == "https://repo.anaconda.com/conda-forge");
            REQUIRE(uc.platform_filters() == PlatformSet{ "osx-64" });
        }

        SECTION("https://repo.anaconda.com/conda-forge[win-64|noarch]")
        {
            const auto uc = UnresolvedChannel::parse(
                                "https://repo.anaconda.com/conda-forge[win-64|noarch]"
            )
                                .value();
            REQUIRE(uc.type() == Type::URL);
            REQUIRE(uc.location() == "https://repo.anaconda.com/conda-forge");
            REQUIRE(uc.platform_filters() == PlatformSet{ "win-64", "noarch" });
        }

        SECTION("https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda")
        {
            const auto uc = UnresolvedChannel::parse(
                                "https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda"
            )
                                .value();
            REQUIRE(uc.type() == Type::PackageURL);
            REQUIRE(
                uc.location() == "https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda"
            );
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("file:///Users/name/conda")
        {
            const auto uc = UnresolvedChannel::parse("file:///Users/name/conda").value();
            REQUIRE(uc.type() == Type::Path);
            REQUIRE(uc.location() == "file:///Users/name/conda");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("file:///Users/name/conda[linux-64]")
        {
            const auto uc = UnresolvedChannel::parse("file:///Users/name/conda[linux-64]").value();
            REQUIRE(uc.type() == Type::Path);
            REQUIRE(uc.location() == "file:///Users/name/conda");
            REQUIRE(uc.platform_filters() == PlatformSet{ "linux-64" });
        }

        SECTION("file://C:/Users/name/conda")
        {
            if (util::on_win)
            {
                const auto uc = UnresolvedChannel::parse("file://C:/Users/name/conda").value();
                REQUIRE(uc.type() == Type::Path);
                REQUIRE(uc.location() == "file://C:/Users/name/conda");
                REQUIRE(uc.platform_filters() == PlatformSet{});
            }
        }

        SECTION("/Users/name/conda")
        {
            const auto uc = UnresolvedChannel::parse("/Users/name/conda").value();
            REQUIRE(uc.type() == Type::Path);
            REQUIRE(uc.location() == "/Users/name/conda");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("./folder/../folder/.")
        {
            const auto uc = UnresolvedChannel::parse("./folder/../folder/.").value();
            REQUIRE(uc.type() == Type::Path);
            REQUIRE(uc.location() == "./folder");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("./folder/subfolder/")
        {
            const auto uc = UnresolvedChannel::parse("./folder/subfolder/").value();
            REQUIRE(uc.type() == Type::Path);
            REQUIRE(uc.location() == "./folder/subfolder");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("~/folder/")
        {
            const auto uc = UnresolvedChannel::parse("~/folder/").value();
            REQUIRE(uc.type() == Type::Path);
            REQUIRE(uc.location() == "~/folder");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("/tmp/pkg-0.0-bld.tar.bz2")
        {
            const auto uc = UnresolvedChannel::parse("/tmp/pkg-0.0-bld.tar.bz2").value();
            REQUIRE(uc.type() == Type::PackagePath);
            REQUIRE(uc.location() == "/tmp/pkg-0.0-bld.tar.bz2");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("C:/tmp//pkg-0.0-bld.tar.bz2")
        {
            const auto uc = UnresolvedChannel::parse("C:/tmp//pkg-0.0-bld.tar.bz2").value();
            REQUIRE(uc.type() == Type::PackagePath);
            REQUIRE(uc.location() == "C:/tmp/pkg-0.0-bld.tar.bz2");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION(R"(C:\tmp\pkg-0.0-bld.tar.bz2)")
        {
            if (util::on_win)
            {
                const auto uc = UnresolvedChannel::parse(R"(C:\tmp\pkg-0.0-bld.tar.bz2)").value();
                REQUIRE(uc.type() == Type::PackagePath);
                REQUIRE(uc.location() == "C:/tmp/pkg-0.0-bld.tar.bz2");
                REQUIRE(uc.platform_filters() == PlatformSet{});
            }
        }

        SECTION("conda-forge")
        {
            const auto uc = UnresolvedChannel::parse("conda-forge").value();
            REQUIRE(uc.type() == Type::Name);
            REQUIRE(uc.location() == "conda-forge");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("repo.anaconda.com")
        {
            const auto uc = UnresolvedChannel::parse("repo.anaconda.com").value();
            // Unintuitive but correct type, this is not a URL. Better explicit than clever.
            REQUIRE(uc.type() == Type::Name);
            REQUIRE(uc.location() == "repo.anaconda.com");
            REQUIRE(uc.platform_filters() == PlatformSet{});
        }

        SECTION("conda-forge/linux-64")
        {
            const auto uc = UnresolvedChannel::parse("conda-forge/linux-64").value();
            REQUIRE(uc.type() == Type::Name);
            REQUIRE(uc.location() == "conda-forge");
            REQUIRE(uc.platform_filters() == PlatformSet{ "linux-64" });
        }

        SECTION("conda-forge[linux-avx512]")
        {
            const auto uc = UnresolvedChannel::parse("conda-forge[linux-avx512]").value();
            REQUIRE(uc.type() == Type::Name);
            REQUIRE(uc.location() == "conda-forge");
            REQUIRE(uc.platform_filters() == PlatformSet{ "linux-avx512" });
        }

        SECTION("conda-forge[]")
        {
            const auto uc = UnresolvedChannel::parse("conda-forge[linux-64]").value();
            REQUIRE(uc.type() == Type::Name);
            REQUIRE(uc.location() == "conda-forge");
            REQUIRE(uc.platform_filters() == PlatformSet{ "linux-64" });
        }

        SECTION("conda-forge/linux-64/label/foo_dev")
        {
            const auto uc = UnresolvedChannel::parse("conda-forge/linux-64/label/foo_dev").value();
            REQUIRE(uc.type() == Type::Name);
            REQUIRE(uc.location() == "conda-forge/label/foo_dev");
            REQUIRE(uc.platform_filters() == PlatformSet{ "linux-64" });
        }

        SECTION("conda-forge/label/foo_dev[linux-64]")
        {
            const auto uc = UnresolvedChannel::parse("conda-forge/label/foo_dev[linux-64]").value();
            REQUIRE(uc.type() == Type::Name);
            REQUIRE(uc.location() == "conda-forge/label/foo_dev");
            REQUIRE(uc.platform_filters() == PlatformSet{ "linux-64" });
        }
    }

    TEST_CASE("str")
    {
        REQUIRE(UnresolvedChannel("location", {}, Type::Name).str() == "location");
        REQUIRE(
            UnresolvedChannel("location", { "linux-64", "noarch" }, Type::Name).str()
            == "location[linux-64,noarch]"
        );
    }

    TEST_CASE("Comparability and hashability")
    {
        auto uc1 = UnresolvedChannel::parse("conda-forge").value();
        auto uc2 = UnresolvedChannel::parse("conda-forge").value();
        auto uc3 = UnresolvedChannel::parse("conda-forge/linux-64").value();

        REQUIRE(uc1 == uc2);
        REQUIRE(uc1 != uc3);

        auto hash_fn = std::hash<UnresolvedChannel>();
        REQUIRE(hash_fn(uc1) == hash_fn(uc2));
        REQUIRE(hash_fn(uc1) != hash_fn(uc3));
    }
}
