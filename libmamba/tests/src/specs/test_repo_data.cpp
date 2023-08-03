// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "mamba/specs/repo_data.hpp"

using namespace mamba::specs;
namespace nl = nlohmann;

TEST_SUITE("repo_data")
{
    TEST_CASE("RepoDataPackage_to_json")
    {
        auto p = RepoDataPackage();
        p.name = "mamba";
        p.version = Version::parse("1.0.0");
        p.build_string = "bld";
        p.build_number = 3;
        p.subdir = "folder";
        p.md5 = "ffsd";
        p.noarch = NoArchType::Python;

        nl::json const j = p;
        CHECK_EQ(j.at("name"), p.name);
        CHECK_EQ(j.at("version"), p.version.str());
        CHECK_EQ(j.at("build"), p.build_string);
        CHECK_EQ(j.at("build_number"), p.build_number);
        CHECK_EQ(j.at("subdir"), p.subdir);
        CHECK_EQ(j.at("md5"), p.md5);
        CHECK(j.at("sha256").is_null());
        CHECK_EQ(j.at("noarch"), "python");
    }

    TEST_CASE("RepoDataPackage_from_json")
    {
        auto j = nl::json::object();
        j["name"] = "mamba";
        j["version"] = "1.1.0";
        j["build"] = "foo1";
        j["build_number"] = 2;
        j["subdir"] = "folder";
        j["platform"] = nullptr;
        j["depends"] = nl::json::array({ "libsolv>=1.0" });
        j["constrains"] = nl::json::array();
        j["track_features"] = nl::json::array();
        {
            const auto p = j.get<RepoDataPackage>();
            CHECK_EQ(p.name, j.at("name"));
            // Note Version::parse is not injective
            CHECK_EQ(p.version.str(), j.at("version"));
            CHECK_EQ(p.build_string, j.at("build"));
            CHECK_EQ(p.build_number, j.at("build_number"));
            CHECK_EQ(p.subdir, j.at("subdir"));
            CHECK_FALSE(p.md5.has_value());
            CHECK_FALSE(p.platform.has_value());
            CHECK_EQ(p.depends, decltype(p.depends){ "libsolv>=1.0" });
            CHECK(p.constrains.empty());
            CHECK(p.track_features.empty());
            CHECK_FALSE(p.noarch.has_value());
        }
        j["noarch"] = "python";
        {
            const auto p = j.get<RepoDataPackage>();
            CHECK_EQ(p.noarch, NoArchType::Python);
        }
        // Old beahiour
        j["noarch"] = true;
        {
            const auto p = j.get<RepoDataPackage>();
            CHECK_EQ(p.noarch, NoArchType::Generic);
        }
        j["noarch"] = false;
        {
            const auto p = j.get<RepoDataPackage>();
            CHECK_FALSE(p.noarch.has_value());
        }
    }

    TEST_CASE("RepoData_to_json")
    {
        auto data = RepoData();
        data.version = 1;
        data.info = ChannelInfo{ /* .subdir= */ Platform::linux_64 };
        data.packages = {
            { "mamba-1.0-h12345.tar.bz2", RepoDataPackage{ "mamba" } },
            { "conda-1.0-h54321.tar.bz2", RepoDataPackage{ "conda" } },
        };
        data.removed = { "bad-package-1" };

        const nl::json j = data;
        CHECK_EQ(j.at("version"), data.version);
        CHECK_EQ(j.at("info").at("subdir"), platform_name(data.info.value().subdir));
        CHECK_EQ(
            j.at("packages").at("mamba-1.0-h12345.tar.bz2"),
            data.packages.at("mamba-1.0-h12345.tar.bz2")
        );
        CHECK_EQ(
            j.at("packages").at("conda-1.0-h54321.tar.bz2"),
            data.packages.at("conda-1.0-h54321.tar.bz2")
        );
        CHECK_EQ(j.at("removed"), std::vector{ "bad-package-1" });
    }

    TEST_CASE("RepoData_from_json")
    {
        auto j = nl::json::object();
        j["version"] = 1;
        j["info"]["subdir"] = "osx-arm64";
        j["packages"]["mamba-1.0-h12345.tar.bz2"]["name"] = "mamba";
        j["packages"]["mamba-1.0-h12345.tar.bz2"]["version"] = "1.1.0";
        j["packages"]["mamba-1.0-h12345.tar.bz2"]["build"] = "foo1";
        j["packages"]["mamba-1.0-h12345.tar.bz2"]["build_number"] = 2;
        j["packages"]["mamba-1.0-h12345.tar.bz2"]["subdir"] = "folder";
        j["packages"]["mamba-1.0-h12345.tar.bz2"]["depends"] = nl::json::array({ "libsolv>=1.0" });
        j["packages"]["mamba-1.0-h12345.tar.bz2"]["constrains"] = nl::json::array();
        j["packages"]["mamba-1.0-h12345.tar.bz2"]["track_features"] = nl::json::array();
        j["conda_packages"] = nl::json::object();
        j["removed"][0] = "bad-package.tar.gz";

        const auto data = j.get<RepoData>();
        REQUIRE(data.version.has_value());
        CHECK_EQ(data.version, j["version"]);
        REQUIRE(data.info.has_value());
        CHECK_EQ(platform_name(data.info.value().subdir), j["info"]["subdir"]);
        CHECK_EQ(
            data.packages.at("mamba-1.0-h12345.tar.bz2").name,
            j["packages"]["mamba-1.0-h12345.tar.bz2"]["name"]
        );
        CHECK(data.conda_packages.empty());
        CHECK_EQ(data.removed, j["removed"]);
    }

    TEST_CASE("repodata_json")
    {
        // Maybe not the best way to set this test.
        // ``repodata.json`` of interest are very large files. Should we check them in in VCS?
        // Download them in CMake? Do a specific integration test?
        // Could be downloaded in the tests, but we would like to keep these tests Context-free.
        const char* repodata_file_path = std::getenv("MAMBA_REPODATA_JSON");
        if (repodata_file_path == nullptr)
        {
            return;
        }
        auto repodata_file = std::ifstream(repodata_file_path);
        // Deserialize
        auto data = nl::json::parse(repodata_file).get<RepoData>();
        // Serialize
        const nl::json json = std::move(data);
    }
}
