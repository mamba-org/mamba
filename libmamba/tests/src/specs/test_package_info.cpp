// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "mamba/specs/package_info.hpp"

#include "doctest-printer/vector.hpp"

namespace nl = nlohmann;
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
            CHECK_EQ(pkg.subdir, "linux-64");
            CHECK_EQ(pkg.channel, "https://conda.anaconda.org/conda-forge");
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
            CHECK_EQ(pkg.subdir, "linux-64");
            CHECK_EQ(pkg.channel, "https://conda.anaconda.org/conda-forge");
        }
    }

    TEST_CASE("PackageInfo serialization")
    {
        using StrVec = std::vector<std::string>;

        auto pkg = PackageInfo();
        pkg.name = "foo";
        pkg.version = "4.0";
        pkg.build_string = "mybld";
        pkg.build_number = 5;
        pkg.noarch = NoArchType::Generic;
        pkg.channel = "conda-forge";
        pkg.package_url = "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda";
        pkg.subdir = "linux-64";
        pkg.filename = "foo-4.0-mybld.conda";
        pkg.license = "MIT";
        pkg.size = 3200;
        pkg.timestamp = 4532;
        pkg.sha256 = "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b";
        pkg.md5 = "68b329da9893e34099c7d8ad5cb9c940";
        pkg.track_features = { "mkl", "blas" };
        pkg.depends = { "python>=3.7", "requests" };
        pkg.constrains = { "pip>=2.1" };

        SUBCASE("field")
        {
            CHECK_EQ(pkg.field("name"), "foo");
            CHECK_EQ(pkg.field("version"), "4.0");
            CHECK_EQ(pkg.field("build_string"), "mybld");
            CHECK_EQ(pkg.field("build_number"), "5");
            CHECK_EQ(pkg.field("noarch"), "generic");
            CHECK_EQ(pkg.field("channel"), "conda-forge");
            CHECK_EQ(
                pkg.field("package_url"),
                "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda"
            );
            CHECK_EQ(pkg.field("subdir"), "linux-64");
            CHECK_EQ(pkg.field("filename"), "foo-4.0-mybld.conda");
            CHECK_EQ(pkg.field("license"), "MIT");
            CHECK_EQ(pkg.field("size"), "3200");
            CHECK_EQ(pkg.field("timestamp"), "4532");
        }

        SUBCASE("to_json")
        {
            const auto j = nl::json(pkg);
            CHECK_EQ(j.at("name"), "foo");
            CHECK_EQ(j.at("version"), "4.0");
            CHECK_EQ(j.at("build_string"), "mybld");
            CHECK_EQ(j.at("build_number"), 5);
            CHECK_EQ(j.at("noarch"), "generic");
            CHECK_EQ(j.at("channel"), "conda-forge");
            CHECK_EQ(j.at("url"), "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda");
            CHECK_EQ(j.at("subdir"), "linux-64");
            CHECK_EQ(j.at("fn"), "foo-4.0-mybld.conda");
            CHECK_EQ(j.at("license"), "MIT");
            CHECK_EQ(j.at("size"), 3200);
            CHECK_EQ(j.at("timestamp"), 4532);
            CHECK_EQ(j.at("sha256"), "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b");
            CHECK_EQ(j.at("md5"), "68b329da9893e34099c7d8ad5cb9c940");
            CHECK_EQ(j.at("track_features"), "mkl,blas");
            CHECK_EQ(j.at("depends"), StrVec{ "python>=3.7", "requests" });
            CHECK_EQ(j.at("constrains"), StrVec{ "pip>=2.1" });
        }

        SUBCASE("from_json")
        {
            auto j = nl::json::object();
            j["name"] = "foo";
            j["version"] = "4.0";
            j["build_string"] = "mybld";
            j["build_number"] = 5;
            j["noarch"] = "generic";
            j["channel"] = "conda-forge";
            j["url"] = "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda";
            j["subdir"] = "linux-64";
            j["fn"] = "foo-4.0-mybld.conda";
            j["license"] = "MIT";
            j["size"] = 3200;
            j["timestamp"] = 4532;
            j["sha256"] = "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b";
            j["md5"] = "68b329da9893e34099c7d8ad5cb9c940";
            j["track_features"] = "mkl,blas";
            j["depends"] = StrVec{ "python>=3.7", "requests" };
            j["constrains"] = StrVec{ "pip>=2.1" };

            CHECK_EQ(j.get<PackageInfo>(), pkg);

            SUBCASE("noarch")
            {
                j["noarch"] = "Python";
                CHECK_EQ(j.get<PackageInfo>().noarch, NoArchType::Python);
                j["noarch"] = true;
                CHECK_EQ(j.get<PackageInfo>().noarch, NoArchType::Generic);
                j["noarch"] = false;
                CHECK_EQ(j.get<PackageInfo>().noarch, NoArchType::No);
                j["noarch"] = nullptr;
                CHECK_EQ(j.get<PackageInfo>().noarch, NoArchType::No);
                j.erase("noarch");
                CHECK_EQ(j.get<PackageInfo>().noarch, NoArchType::No);
            }

            SUBCASE("track_features")
            {
                j["track_features"] = "python";
                CHECK_EQ(j.get<PackageInfo>().track_features, StrVec{ "python" });
                j["track_features"] = "python,mkl";
                CHECK_EQ(j.get<PackageInfo>().track_features, StrVec{ "python", "mkl" });
                j.erase("track_features");
                CHECK_EQ(j.get<PackageInfo>().track_features, StrVec{});
                j["track_features"] = nl::json::array({ "py", "malloc" });
                CHECK_EQ(j.get<PackageInfo>().track_features, StrVec{ "py", "malloc" });
            }
        }
    }
}
