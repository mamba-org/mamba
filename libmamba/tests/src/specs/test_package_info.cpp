// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <vector>

#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>

#include "mamba/specs/package_info.hpp"

#include "doctest-printer/vector.hpp"

namespace nl = nlohmann;
using namespace mamba::specs;

namespace
{
    TEST_CASE("PackageInfo::from_url")
    {
        SECTION("https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda")
        {
            static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda";

            auto pkg = PackageInfo::from_url(url).value();

            REQUIRE(pkg.name == "pkg");
            REQUIRE(pkg.version == "6.4");
            REQUIRE(pkg.build_string == "bld");
            REQUIRE(pkg.filename == "pkg-6.4-bld.conda");
            REQUIRE(pkg.package_url == url);
            REQUIRE(pkg.md5 == "");
            REQUIRE(pkg.platform == "linux-64");
            REQUIRE(pkg.channel == "https://conda.anaconda.org/conda-forge");
        }

        SECTION("https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda#7dbaa197d7ba6032caf7ae7f32c1efa0"
        )
        {
            static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda#7dbaa197d7ba6032caf7ae7f32c1efa0";

            auto pkg = PackageInfo::from_url(url).value();

            REQUIRE(pkg.name == "pkg");
            REQUIRE(pkg.version == "6.4");
            REQUIRE(pkg.build_string == "bld");
            REQUIRE(pkg.filename == "pkg-6.4-bld.conda");
            REQUIRE(pkg.package_url == url.substr(0, url.rfind('#')));
            REQUIRE(pkg.md5 == url.substr(url.rfind('#') + 1));
            REQUIRE(pkg.platform == "linux-64");
            REQUIRE(pkg.channel == "https://conda.anaconda.org/conda-forge");
        }

        SECTION("https://conda.anaconda.org/conda-forge/linux-64/pkg.conda")
        {
            static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/pkg.conda";
            REQUIRE_FALSE(PackageInfo::from_url(url).has_value());
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
        pkg.platform = "linux-64";
        pkg.filename = "foo-4.0-mybld.conda";
        pkg.license = "MIT";
        pkg.size = 3200;
        pkg.timestamp = 4532;
        pkg.sha256 = "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b";
        pkg.signatures = R"("signatures": { "some_file.tar.bz2": { "a133184c9c7a651f55db194031a6c1240b798333923dc9319d1fe2c94a1242d": { "signature": "7a67a875d0454c14671d960a02858e059d154876dab6b3873304a27102063c9c25"}}})";
        pkg.md5 = "68b329da9893e34099c7d8ad5cb9c940";
        pkg.track_features = { "mkl", "blas" };
        pkg.dependencies = { "python>=3.7", "requests" };
        pkg.constrains = { "pip>=2.1" };

        SECTION("field")
        {
            REQUIRE(pkg.field("name") == "foo");
            REQUIRE(pkg.field("version") == "4.0");
            REQUIRE(pkg.field("build_string") == "mybld");
            REQUIRE(pkg.field("build_number") == "5");
            REQUIRE(pkg.field("noarch") == "generic");
            REQUIRE(pkg.field("channel") == "conda-forge");
            REQUIRE(
                pkg.field("package_url")
                == "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda"
            );
            REQUIRE(pkg.field("subdir") == "linux-64");
            REQUIRE(pkg.field("filename") == "foo-4.0-mybld.conda");
            REQUIRE(pkg.field("license") == "MIT");
            REQUIRE(pkg.field("size") == "3200");
            REQUIRE(pkg.field("timestamp") == "4532");
        }

        SECTION("to_json")
        {
            const auto j = nl::json(pkg);
            REQUIRE(j.at("name") == "foo");
            REQUIRE(j.at("version") == "4.0");
            REQUIRE(j.at("build_string") == "mybld");
            REQUIRE(j.at("build_number") == 5);
            REQUIRE(j.at("noarch") == "generic");
            REQUIRE(j.at("channel") == "conda-forge");
            REQUIRE(j.at("url") == "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda");
            REQUIRE(j.at("subdir") == "linux-64");
            REQUIRE(j.at("fn") == "foo-4.0-mybld.conda");
            REQUIRE(j.at("license") == "MIT");
            REQUIRE(j.at("size") == 3200);
            REQUIRE(j.at("timestamp") == 4532);
            REQUIRE(
                j.at("sha256") == "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b"
            );
            REQUIRE(
                j.at("signatures")
                == R"("signatures": { "some_file.tar.bz2": { "a133184c9c7a651f55db194031a6c1240b798333923dc9319d1fe2c94a1242d": { "signature": "7a67a875d0454c14671d960a02858e059d154876dab6b3873304a27102063c9c25"}}})"
            );
            REQUIRE(j.at("md5") == "68b329da9893e34099c7d8ad5cb9c940");
            REQUIRE(j.at("track_features") == "mkl,blas");
            REQUIRE(j.at("depends") == StrVec{ "python>=3.7", "requests" });
            REQUIRE(j.at("constrains") == StrVec{ "pip>=2.1" });
        }

        SECTION("from_json")
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
            j["signatures"] = R"("signatures": { "some_file.tar.bz2": { "a133184c9c7a651f55db194031a6c1240b798333923dc9319d1fe2c94a1242d": { "signature": "7a67a875d0454c14671d960a02858e059d154876dab6b3873304a27102063c9c25"}}})";
            j["md5"] = "68b329da9893e34099c7d8ad5cb9c940";
            j["track_features"] = "mkl,blas";
            j["depends"] = StrVec{ "python>=3.7", "requests" };
            j["constrains"] = StrVec{ "pip>=2.1" };

            REQUIRE(j.get<PackageInfo>() == pkg);

            SECTION("noarch")
            {
                j["noarch"] = "Python";
                REQUIRE(j.get<PackageInfo>().noarch == NoArchType::Python);
                j["noarch"] = true;
                REQUIRE(j.get<PackageInfo>().noarch == NoArchType::Generic);
                j["noarch"] = false;
                REQUIRE(j.get<PackageInfo>().noarch == NoArchType::No);
                j["noarch"] = nullptr;
                REQUIRE(j.get<PackageInfo>().noarch == NoArchType::No);
                j.erase("noarch");
                REQUIRE(j.get<PackageInfo>().noarch == NoArchType::No);
            }

            SECTION("track_features")
            {
                j["track_features"] = "python";
                REQUIRE(j.get<PackageInfo>().track_features == StrVec{ "python" });
                j["track_features"] = "python,mkl";
                REQUIRE(j.get<PackageInfo>().track_features == StrVec{ "python", "mkl" });
                j.erase("track_features");
                REQUIRE(j.get<PackageInfo>().track_features == StrVec{});
                j["track_features"] = nl::json::array({ "py", "malloc" });
                REQUIRE(j.get<PackageInfo>().track_features == StrVec{ "py", "malloc" });
            }

            SECTION("equality_operator")
            {
                REQUIRE(j.get<PackageInfo>() == pkg);
            }

            SECTION("inequality_operator")
            {
                REQUIRE_FALSE(j.get<PackageInfo>() != pkg);
            }
        }

        SECTION("PackageInfo comparability and hashability")
        {
            auto pkg2 = PackageInfo();
            pkg2.name = "foo";
            pkg2.version = "4.0";
            pkg2.build_string = "mybld";
            pkg2.build_number = 5;
            pkg2.noarch = NoArchType::Generic;
            pkg2.channel = "conda-forge";
            pkg2.package_url = "https://repo.mamba.pm/conda-forge/linux-64/foo-4.0-mybld.conda";
            pkg2.platform = "linux-64";
            pkg2.filename = "foo-4.0-mybld.conda";
            pkg2.license = "MIT";
            pkg2.size = 3200;
            pkg2.timestamp = 4532;
            pkg2.sha256 = "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b";
            pkg2.signatures = R"("signatures": { "some_file.tar.bz2": { "a133184c9c7a651f55db194031a6c1240b798333923dc9319d1fe2c94a1242d": { "signature": "7a67a875d0454c14671d960a02858e059d154876dab6b3873304a27102063c9c25"}}})";
            pkg2.md5 = "68b329da9893e34099c7d8ad5cb9c940";
            pkg2.track_features = { "mkl", "blas" };
            pkg2.dependencies = { "python>=3.7", "requests" };
            pkg2.constrains = { "pip>=2.1" };

            auto hash_fn = std::hash<PackageInfo>{};

            REQUIRE(pkg == pkg2);
            REQUIRE(hash_fn(pkg) == hash_fn(pkg2));


            pkg2.md5[0] = '0';

            REQUIRE(pkg != pkg2);
            REQUIRE(hash_fn(pkg) != hash_fn(pkg2));
        }
    }
}
