// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <filesystem>
#include <fstream>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "mamba/specs/package_info.hpp"

using namespace mamba::specs;
namespace nl = nlohmann;

TEST_SUITE("package_info")
{
    TEST_CASE("PackageInfo to JSON")
    {
        auto p = PackageInfo{};
        p.pkg.name = "mamba";
        p.file_name = "mamba-1.0.0-bld.tgz";
        p.file_url = "https://conda.anaconda.org/conda-forge/linux-64/mamba-1.0.0-bld.tgz";
        p.channel = "https://conda.anaconda.org/conda-forge/linux-64";

        nl::json const j = p;
        CHECK_EQ(j.at("name"), p.pkg.name);
        CHECK_EQ(j.at("fn"), p.file_name);
        CHECK_EQ(j.at("url"), p.file_url);
        CHECK_EQ(j.at("channel"), p.channel);
    }

    TEST_CASE("Package from JSON")
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
        j["fn"] = "mamba-1.0.0-bld.tgz";
        j["url"] = "https://conda.anaconda.org/conda-forge/linux-64/mamba-1.0.0-bld.tgz";
        j["channel"] = "https://conda.anaconda.org/conda-forge/linux-64";

        const auto p = j.get<PackageInfo>();
        CHECK_EQ(p.pkg.name, j["name"]);
        CHECK_EQ(p.file_name, j["fn"]);
        CHECK_EQ(p.file_url, j["url"]);
        CHECK_EQ(p.channel, j["channel"]);
    }

    TEST_CASE("Can parse repodata_record.json")
    {
        // Maybe not the best way to set this test.
        const char* cache_dir_path = std::getenv("TEST_MAMBA_CACHE");
        if (cache_dir_path == nullptr)
        {
            return;
        }
        for (const auto& pkg_entry : std::filesystem::directory_iterator{ cache_dir_path })
        {
            const auto repodata_record_file = pkg_entry.path() / "info" / "repodata_record.json";
            if (std::filesystem::exists(repodata_record_file))
            {
                auto repodata_record = std::ifstream(repodata_record_file);
                // Deserialize
                auto data = nl::json::parse(repodata_record).get<PackageInfo>();
                // Serialize
                const nl::json json = data;
            }
        }
    }
}
