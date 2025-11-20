// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <filesystem>
#include <fstream>
#include <functional>

#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>

#include "mamba/core/util.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

#include "mambatests.hpp"

using namespace mamba;
using namespace mamba::solver;

namespace
{
    auto mkpkg(std::string name, std::string version, std::vector<std::string> deps = {})
        -> specs::PackageInfo
    {
        auto out = specs::PackageInfo();
        out.name = std::move(name);
        out.version = std::move(version);
        out.dependencies = std::move(deps);
        return out;
    }
}

namespace
{
    using PackageInfo = specs::PackageInfo;

    TEST_CASE("Create a database", "[mamba::solver][mamba::solver::libsolv]")
    {
        const auto matchspec_parser = GENERATE(
            libsolv::MatchSpecParser::Libsolv,
            libsolv::MatchSpecParser::Mixed,
            libsolv::MatchSpecParser::Mamba
        );
        CAPTURE(matchspec_parser);

        auto db = libsolv::Database({}, { matchspec_parser });
        REQUIRE(std::is_move_constructible_v<libsolv::Database>);
        REQUIRE(db.repo_count() == 0);

        SECTION("Add repo from packages")
        {
            auto pkgs = std::array{
                mkpkg("x", "1.0"),
                mkpkg("x", "2.0"),
                mkpkg("z", "1.0", { "x>=1.0" }),
            };
            auto repo1 = db.add_repo_from_packages(pkgs, "repo1");
            REQUIRE(db.repo_count() == 1);
            REQUIRE(db.package_count() == 3);
            REQUIRE(repo1.package_count() == 3);

            SECTION("Mark as installed repo")
            {
                REQUIRE_FALSE(db.installed_repo().has_value());
                db.set_installed_repo(repo1);
                REQUIRE(db.installed_repo().value() == repo1);

                SECTION("Remove repo")
                {
                    db.remove_repo(repo1);
                    REQUIRE(db.repo_count() == 0);
                    REQUIRE_FALSE(db.installed_repo().has_value());
                    REQUIRE(db.package_count() == 0);
                }
            }

            SECTION("Serialize repo")
            {
                auto tmp_dir = TemporaryDirectory();
                auto solv_file = tmp_dir.path() / "repo1.solv";

                auto origin = libsolv::RepodataOrigin{
                    /* .url= */ "https://repo.mamba.pm",
                    /* .etag= */ "etag",
                    /* .mod= */ "Fri, 11 Feb 2022 13:52:44 GMT",
                };
                auto repo1_copy = db.native_serialize_repo(repo1, solv_file, origin);
                REQUIRE(repo1_copy == repo1);

                SECTION("Read serialized repo")
                {
                    auto repo2 = db.add_repo_from_native_serialization(solv_file, origin, "conda-forge")
                                     .value();
                    REQUIRE(repo2.name() == origin.url);
                    REQUIRE(repo2.package_count() == repo1.package_count());
                    REQUIRE(repo2 != repo1);
                    REQUIRE(db.package_count() == repo1.package_count() + repo2.package_count());
                }

                SECTION("Fail reading outdated repo")
                {
                    for (auto attr : {
                             &libsolv::RepodataOrigin::url,
                             &libsolv::RepodataOrigin::etag,
                             &libsolv::RepodataOrigin::mod,
                         })
                    {
                        auto expected = origin;
                        std::invoke(attr, expected) = "";
                        auto maybe = db.add_repo_from_native_serialization(
                            solv_file,
                            expected,
                            "conda-forge"
                        );
                        REQUIRE_FALSE(maybe.has_value());
                    }
                }
            }

            SECTION("Iterate over packages")
            {
                auto repo2 = db.add_repo_from_packages(std::array{ mkpkg("z", "2.0") }, "repo1");

                SECTION("In a given repo")
                {
                    std::size_t count = 0;
                    db.for_each_package_in_repo(
                        repo2,
                        [&](const auto& p)
                        {
                            count++;
                            REQUIRE(p.name == "z");
                            REQUIRE(p.version == "2.0");
                        }
                    );
                    REQUIRE(count == 1);
                }

                SECTION("Matching a MatchSpec in multiple repos")
                {
                    std::size_t count = 0;
                    db.for_each_package_matching(
                        specs::MatchSpec::parse("z").value(),
                        [&](const auto& p)
                        {
                            count++;
                            REQUIRE(p.name == "z");
                        }
                    );
                    REQUIRE(count == 2);
                }

                SECTION("Matching a strict MatchSpec")
                {
                    std::size_t count = 0;
                    db.for_each_package_matching(
                        specs::MatchSpec::parse("z>1.0").value(),
                        [&](const auto& p)
                        {
                            count++;
                            REQUIRE(p.name == "z");
                        }
                    );
                    REQUIRE(count == 1);
                }

                SECTION("Depending on a given dependency")
                {
                    // Complex repoqueries do not work with namespace callbacks
                    if (matchspec_parser != libsolv::MatchSpecParser::Libsolv)
                    {
                        return;
                    }

                    std::size_t count = 0;
                    db.for_each_package_depending_on(
                        specs::MatchSpec::parse("x").value(),
                        [&](const auto& p)
                        {
                            count++;
                            REQUIRE(util::any_starts_with(p.dependencies, "x"));
                        }
                    );
                    REQUIRE(count == 1);
                }
            }
        }

        SECTION("Add repo from repodata with no extra pip")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-numpy-linux-64.json";
            auto repo1 = db.add_repo_from_repodata_json(
                repodata,
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::No
            );
            REQUIRE(repo1.has_value());

            REQUIRE(repo1->package_count() == 33);

            auto found_python = false;
            db.for_each_package_matching(
                specs::MatchSpec::parse("python").value(),
                [&](const specs::PackageInfo& pkg)
                {
                    found_python = true;
                    for (const auto& dep : pkg.dependencies)
                    {
                        REQUIRE_FALSE(util::contains(dep, "pip"));
                    }
                }
            );
            REQUIRE(found_python);
        }

        SECTION("Add repo from repodata with extra pip")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-numpy-linux-64.json";
            auto repo1 = db.add_repo_from_repodata_json(
                repodata,
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::Yes
            );
            REQUIRE(repo1.has_value());

            REQUIRE(repo1->package_count() == 33);

            auto found_python = false;
            db.for_each_package_matching(
                specs::MatchSpec::parse("python").value(),
                [&](const specs::PackageInfo& pkg)
                {
                    found_python = true;
                    auto found_pip = false;
                    for (const auto& dep : pkg.dependencies)
                    {
                        found_pip |= util::contains(dep, "pip");
                    }
                    REQUIRE(found_pip);
                }
            );
            REQUIRE(found_python);
        }

        SECTION("Add repo from repodata only .tar.bz2")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-numpy-linux-64.json";
            auto repo1 = db.add_repo_from_repodata_json(
                repodata,
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::TarBz2Only
            );
            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 4);
        }

        SECTION("Add repo from repodata only .conda")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-numpy-linux-64.json";
            auto repo1 = db.add_repo_from_repodata_json(
                repodata,
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOnly
            );
            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 30);
        }

        SECTION("Add repo from repodata .conda and .tar.bz2")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-numpy-linux-64.json";
            auto repo1 = db.add_repo_from_repodata_json(
                repodata,
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaAndTarBz2
            );
            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 34);
        }

        SECTION("Add repo from repodata .conda or else .tar.bz2")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-numpy-linux-64.json";
            auto repo1 = db.add_repo_from_repodata_json(
                repodata,
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOrElseTarBz2
            );
            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 33);
        }

        SECTION("Add repo from repodata with verifying packages signatures")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-numpy-linux-64.json";
            SECTION("Using mamba parser")
            {
                auto repo1 = db.add_repo_from_repodata_json(
                    repodata,
                    "https://conda.anaconda.org/conda-forge/linux-64",
                    "conda-forge",
                    libsolv::PipAsPythonDependency::No,
                    libsolv::PackageTypes::CondaOrElseTarBz2,
                    libsolv::VerifyPackages::Yes,
                    libsolv::RepodataParser::Mamba
                );
                REQUIRE(repo1.has_value());
                REQUIRE(repo1->package_count() == 33);

                db.for_each_package_in_repo(
                    repo1.value(),
                    [&](const auto& p)
                    {
                        if (p.name == "_libgcc_mutex")
                        {
                            REQUIRE(
                                p.signatures
                                == R"({"signatures":{"0b7a133184c9c98333923dhfdg86031adc5db1fds54kfga941fe2c94a12fdjg8":{"signature":"0b83c91ddd8b81bbc7a67a586bde4a271bd8f97069c25306870e314f3664ab02083c91ddd8b0dfjsg763jbd0jh14671d960bb303d1eb787307c04c414ediz95a"}}})"
                            );
                        }
                        else if (p.name == "bzip2")
                        {
                            REQUIRE(
                                p.signatures
                                == R"({"signatures":{"f7a651f55db194031a6c1240b7a133184c9c98333923dc9319d1fe2c94a1242d":{"signature":"058bf4b5d5cb738736870e314f3664b83c91ddd8b81bbc7a67a875d0454c14671d960a02858e059d154876dab6bde853d763c1a3bd8f97069c25304a2710200d"}}})"
                            );
                        }
                        else
                        {
                            REQUIRE(p.signatures == "");
                        }
                    }
                );
            }

            SECTION("Using libsolv parser")
            {
                auto repo1 = db.add_repo_from_repodata_json(
                    repodata,
                    "https://conda.anaconda.org/conda-forge/linux-64",
                    "conda-forge",
                    libsolv::PipAsPythonDependency::No,
                    libsolv::PackageTypes::CondaOrElseTarBz2,
                    libsolv::VerifyPackages::Yes,
                    libsolv::RepodataParser::Libsolv
                );

                // Libsolv repodata parser only works with its own matchspec
                if (matchspec_parser != libsolv::MatchSpecParser::Libsolv)
                {
                    REQUIRE_FALSE(repo1.has_value());
                    return;
                }

                REQUIRE(repo1.has_value());
                REQUIRE(repo1->package_count() == 33);

                db.for_each_package_in_repo(
                    repo1.value(),
                    [&](const auto& p)
                    {
                        if (p.name == "_libgcc_mutex")
                        {
                            REQUIRE_THAT(
                                p.signatures.c_str(),
                                Catch::Matchers::ContainsSubstring(
                                    R"("signatures":{"0b7a133184c9c98333923dhfdg86031adc5db1fds54kfga941fe2c94a12fdjg8":{"signature":"0b83c91ddd8b81bbc7a67a586bde4a271bd8f97069c25306870e314f3664ab02083c91ddd8b0dfjsg763jbd0jh14671d960bb303d1eb787307c04c414ediz95a"}})"
                                )
                            );
                        }
                        else if (p.name == "bzip2")
                        {
                            REQUIRE_THAT(
                                p.signatures.c_str(),
                                Catch::Matchers::ContainsSubstring(
                                    R"("signatures":{"f7a651f55db194031a6c1240b7a133184c9c98333923dc9319d1fe2c94a1242d":{"signature":"058bf4b5d5cb738736870e314f3664b83c91ddd8b81bbc7a67a875d0454c14671d960a02858e059d154876dab6bde853d763c1a3bd8f97069c25304a2710200d"}})"
                                )
                            );
                        }
                        else
                        {
                            REQUIRE(p.signatures == "");
                        }
                    }
                );
            }
        }

        SECTION("Add repo from repodata without verifying packages signatures")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-numpy-linux-64.json";
            SECTION("Using mamba parser")
            {
                auto repo1 = db.add_repo_from_repodata_json(
                    repodata,
                    "https://conda.anaconda.org/conda-forge/linux-64",
                    "conda-forge",
                    libsolv::PipAsPythonDependency::No,
                    libsolv::PackageTypes::CondaOrElseTarBz2,
                    libsolv::VerifyPackages::No,
                    libsolv::RepodataParser::Mamba
                );
                REQUIRE(repo1.has_value());
                REQUIRE(repo1->package_count() == 33);

                db.for_each_package_in_repo(
                    repo1.value(),
                    [&](const auto& p) { REQUIRE(p.signatures == ""); }
                );
            }

            SECTION("Using libsolv parser")
            {
                auto repo1 = db.add_repo_from_repodata_json(
                    repodata,
                    "https://conda.anaconda.org/conda-forge/linux-64",
                    "conda-forge",
                    libsolv::PipAsPythonDependency::No,
                    libsolv::PackageTypes::CondaOrElseTarBz2,
                    libsolv::VerifyPackages::No,
                    libsolv::RepodataParser::Libsolv
                );

                // Libsolv repodata parser only works with its own matchspec
                if (matchspec_parser != libsolv::MatchSpecParser::Libsolv)
                {
                    REQUIRE_FALSE(repo1.has_value());
                    return;
                }

                REQUIRE(repo1.has_value());
                REQUIRE(repo1->package_count() == 33);

                db.for_each_package_in_repo(
                    repo1.value(),
                    [&](const auto& p) { REQUIRE(p.signatures == ""); }
                );
            }
        }

        SECTION("Add repo from repodata with repodata_version 2")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-repodata-version-2.json";
            auto repo1 = db.add_repo_from_repodata_json(
                repodata,
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOrElseTarBz2
            );
            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 2);

            db.for_each_package_in_repo(
                repo1.value(),
                [&](const auto& p)
                {
                    if (p.name == "_libgcc_mutex")
                    {
                        REQUIRE(
                            p.package_url
                            == "https://repo.anaconda.com/repo/main/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
                        );
                    }
                    else if (p.name == "bzip2")
                    {
                        REQUIRE(
                            p.package_url
                            == "https://repo.anaconda.com/repo/main/linux-64/bzip2-1.0.8-hd590300_5.conda"
                        );
                    }
                }
            );
        }

        SECTION("Add repo from repodata with repodata_version 2 with missing base_url")
        {
            const auto repodata = mambatests::test_data_dir
                                  / "repodata/conda-forge-repodata-version-2-missing-base_url.json";
            auto repo1 = db.add_repo_from_repodata_json(
                repodata,
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOrElseTarBz2
            );
            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 2);

            db.for_each_package_in_repo(
                repo1.value(),
                [&](const auto& p)
                {
                    if (p.name == "_libgcc_mutex")
                    {
                        REQUIRE(
                            p.package_url
                            == "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
                        );
                    }
                    else if (p.name == "bzip2")
                    {
                        REQUIRE(
                            p.package_url
                            == "https://conda.anaconda.org/conda-forge/linux-64/bzip2-1.0.8-hd590300_5.conda"
                        );
                    }
                }
            );
        }

        SECTION("Add repo from repodata with conditional dependencies")
        {
            // Create a temporary repodata JSON file with conditional dependencies
            auto temp_dir = std::filesystem::temp_directory_path() / "mamba_test_conditional_deps";
            std::filesystem::create_directories(temp_dir);
            auto repodata_file = temp_dir / "repodata.json";

            // Create repodata JSON with conditional dependencies
            nlohmann::json repodata_json;
            repodata_json["info"]["subdir"] = "linux-64";
            repodata_json["repodata_version"] = 1;
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["name"] = "testpkg";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["version"] = "1.0.0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["depends"] = nlohmann::json::array(
                {
                    "numpy",                               // Regular dependency
                    "pywin32; if __win",                   // Should be skipped on linux-64
                    "unixutils; if __unix",                // Should be added on linux-64
                    "typing-extensions; if python <3.10",  // Complex condition - skipped at parse
                                                           // time
                }
            );
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["constrains"] = nlohmann::json::array(
                {
                    "someconstraint; if __unix",  // Should be added on linux-64
                }
            );

            // Write to file
            {
                std::ofstream file(repodata_file);
                file << repodata_json.dump();
            }

            // Parse with Mamba parser
            auto repo1 = db.add_repo_from_repodata_json(
                repodata_file,
                "https://conda.anaconda.org/test/linux-64",
                "test",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOrElseTarBz2,
                libsolv::VerifyPackages::No,
                libsolv::RepodataParser::Mamba
            );

            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 1);

            // Check that the package was parsed correctly
            db.for_each_package_in_repo(
                repo1.value(),
                [&](const auto& p)
                {
                    if (p.name == "testpkg")
                    {
                        // Should have numpy (regular) and unixutils (__unix condition satisfied)
                        // Should NOT have pywin32 (__win condition not satisfied)
                        // Should NOT have typing-extensions (complex condition skipped)
                        REQUIRE(p.dependencies.size() == 2);

                        // Parse dependencies as MatchSpecs to check names (they may be in "name *"
                        // format)
                        std::vector<std::string> dep_names;
                        for (const auto& dep_str : p.dependencies)
                        {
                            auto ms = specs::MatchSpec::parse(dep_str);
                            if (ms.has_value())
                            {
                                dep_names.push_back(ms->name().to_string());
                            }
                        }

                        REQUIRE(
                            std::find(dep_names.begin(), dep_names.end(), "numpy") != dep_names.end()
                        );
                        REQUIRE(
                            std::find(dep_names.begin(), dep_names.end(), "unixutils")
                            != dep_names.end()
                        );
                        REQUIRE(
                            std::find(dep_names.begin(), dep_names.end(), "pywin32") == dep_names.end()
                        );
                        REQUIRE(
                            std::find(dep_names.begin(), dep_names.end(), "typing-extensions")
                            == dep_names.end()
                        );

                        // Constraint with __unix should be added
                        REQUIRE(p.constrains.size() == 1);
                        std::vector<std::string> cons_names;
                        for (const auto& cons_str : p.constrains)
                        {
                            auto ms = specs::MatchSpec::parse(cons_str);
                            if (ms.has_value())
                            {
                                cons_names.push_back(ms->name().to_string());
                            }
                        }
                        REQUIRE(
                            std::find(cons_names.begin(), cons_names.end(), "someconstraint")
                            != cons_names.end()
                        );
                    }
                }
            );

            // Cleanup
            std::filesystem::remove_all(temp_dir);
        }

        SECTION("Add repo from repodata with conditional dependencies on Windows platform")
        {
            // Create a temporary repodata JSON file with conditional dependencies for Windows
            auto temp_dir = std::filesystem::temp_directory_path()
                            / "mamba_test_conditional_deps_win";
            std::filesystem::create_directories(temp_dir);
            auto repodata_file = temp_dir / "repodata.json";

            // Create repodata JSON with conditional dependencies for Windows
            nlohmann::json repodata_json;
            repodata_json["info"]["subdir"] = "win-64";
            repodata_json["repodata_version"] = 1;
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["name"] = "testpkg";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["version"] = "1.0.0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["subdir"] = "win-64";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["depends"] = nlohmann::json::array(
                {
                    "numpy",                 // Regular dependency
                    "pywin32; if __win",     // Should be added on win-64
                    "unixutils; if __unix",  // Should be skipped on win-64
                }
            );

            // Write to file
            {
                std::ofstream file(repodata_file);
                file << repodata_json.dump();
            }

            // Parse with Mamba parser
            auto repo1 = db.add_repo_from_repodata_json(
                repodata_file,
                "https://conda.anaconda.org/test/win-64",
                "test",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOrElseTarBz2,
                libsolv::VerifyPackages::No,
                libsolv::RepodataParser::Mamba
            );

            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 1);

            // Check that the package was parsed correctly
            db.for_each_package_in_repo(
                repo1.value(),
                [&](const auto& p)
                {
                    if (p.name == "testpkg")
                    {
                        // Should have numpy (regular) and pywin32 (__win condition satisfied)
                        // Should NOT have unixutils (__unix condition not satisfied)
                        REQUIRE(p.dependencies.size() == 2);

                        // Parse dependencies as MatchSpecs to check names (they may be in "name *"
                        // format)
                        std::vector<std::string> dep_names;
                        for (const auto& dep_str : p.dependencies)
                        {
                            auto ms = specs::MatchSpec::parse(dep_str);
                            if (ms.has_value())
                            {
                                dep_names.push_back(ms->name().to_string());
                            }
                        }

                        REQUIRE(
                            std::find(dep_names.begin(), dep_names.end(), "numpy") != dep_names.end()
                        );
                        REQUIRE(
                            std::find(dep_names.begin(), dep_names.end(), "pywin32") != dep_names.end()
                        );
                        REQUIRE(
                            std::find(dep_names.begin(), dep_names.end(), "unixutils")
                            == dep_names.end()
                        );
                    }
                }
            );

            // Cleanup
            std::filesystem::remove_all(temp_dir);
        }

        SECTION("Add repo from repodata with conditional dependencies - complex condition evaluation")
        {
            // Test that complex conditions (e.g., python <3.10) are evaluated against packages in
            // the pool This is step 5: condition evaluation against pool

            auto temp_dir = std::filesystem::temp_directory_path()
                            / "mamba_test_conditional_deps_complex";
            std::filesystem::create_directories(temp_dir);
            auto repodata_file = temp_dir / "repodata.json";

            // Create repodata JSON with a package that has a conditional dependency
            // The condition references python, which will be in the pool
            nlohmann::json repodata_json;
            repodata_json["info"]["subdir"] = "linux-64";
            repodata_json["repodata_version"] = 1;

            // Add python 3.9 package to the repo (this will be in the pool)
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["name"] = "python";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["version"] = "3.9.0";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";

            // Add typing-extensions package
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["name"] = "typing-extensions";
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["version"] = "4.0.0";
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";

            // Add testpkg with conditional dependency on python <3.10
            // Since python 3.9 is in the pool and matches <3.10, typing-extensions should be added
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["name"] = "testpkg";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["version"] = "1.0.0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["depends"] = nlohmann::json::array(
                {
                    "typing-extensions; if python <3.10",  // Should be added because python 3.9 is
                                                           // in pool
                }
            );

            // Write to file
            {
                std::ofstream file(repodata_file);
                file << repodata_json.dump();
            }

            // Parse with Mamba parser
            auto repo1 = db.add_repo_from_repodata_json(
                repodata_file,
                "https://conda.anaconda.org/test/linux-64",
                "test",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOrElseTarBz2,
                libsolv::VerifyPackages::No,
                libsolv::RepodataParser::Mamba
            );

            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 3);  // python, typing-extensions, testpkg

            // Check that testpkg has typing-extensions as a dependency
            // (condition was evaluated and satisfied because python 3.9 is in the pool)
            db.for_each_package_in_repo(
                repo1.value(),
                [&](const auto& p)
                {
                    if (p.name == "testpkg")
                    {
                        REQUIRE(p.dependencies.size() == 1);

                        // Parse dependency to check name
                        auto ms = specs::MatchSpec::parse(p.dependencies[0]);
                        REQUIRE(ms.has_value());
                        REQUIRE(ms->name().to_string() == "typing-extensions");
                    }
                }
            );

            // Cleanup
            std::filesystem::remove_all(temp_dir);
        }

        SECTION("Add repo from repodata with conditional dependencies - condition not satisfied")
        {
            // Test that conditional dependencies are NOT added when condition is not satisfied
            // This is step 5: condition evaluation against pool

            auto temp_dir = std::filesystem::temp_directory_path()
                            / "mamba_test_conditional_deps_not_satisfied";
            std::filesystem::create_directories(temp_dir);
            auto repodata_file = temp_dir / "repodata.json";

            // Create repodata JSON with a package that has a conditional dependency
            // The condition references python >=3.10, but we only have python 3.9 in the pool
            nlohmann::json repodata_json;
            repodata_json["info"]["subdir"] = "linux-64";
            repodata_json["repodata_version"] = 1;

            // Add python 3.9 package to the repo (this will be in the pool)
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["name"] = "python";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["version"] = "3.9.0";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";

            // Add typing-extensions package
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["name"] = "typing-extensions";
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["version"] = "4.0.0";
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["typing-extensions-4.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";

            // Add testpkg with conditional dependency on python >=3.10
            // Since python 3.9 is in the pool and does NOT match >=3.10, typing-extensions should
            // NOT be added
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["name"] = "testpkg";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["version"] = "1.0.0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["depends"] = nlohmann::json::array(
                {
                    "typing-extensions; if python >=3.10",  // Should NOT be added because
                                                            // python 3.9 doesn't match >=3.10
                }
            );

            // Write to file
            {
                std::ofstream file(repodata_file);
                file << repodata_json.dump();
            }

            // Parse with Mamba parser
            auto repo1 = db.add_repo_from_repodata_json(
                repodata_file,
                "https://conda.anaconda.org/test/linux-64",
                "test",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOrElseTarBz2,
                libsolv::VerifyPackages::No,
                libsolv::RepodataParser::Mamba
            );

            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 3);  // python, typing-extensions, testpkg

            // Check that testpkg does NOT have typing-extensions as a dependency
            // (condition was evaluated and NOT satisfied because python 3.9 doesn't match >=3.10)
            db.for_each_package_in_repo(
                repo1.value(),
                [&](const auto& p)
                {
                    if (p.name == "testpkg")
                    {
                        REQUIRE(p.dependencies.empty());  // No dependencies because condition was
                                                          // not satisfied
                    }
                }
            );

            // Cleanup
            std::filesystem::remove_all(temp_dir);
        }

        SECTION("Add repo from repodata with conditional dependencies - AND condition")
        {
            // Test that AND conditions are evaluated correctly
            // This is step 5: condition evaluation against pool with AND logic

            auto temp_dir = std::filesystem::temp_directory_path()
                            / "mamba_test_conditional_deps_and";
            std::filesystem::create_directories(temp_dir);
            auto repodata_file = temp_dir / "repodata.json";

            nlohmann::json repodata_json;
            repodata_json["info"]["subdir"] = "linux-64";
            repodata_json["repodata_version"] = 1;

            // Add python 3.9
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["name"] = "python";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["version"] = "3.9.0";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";

            // Add numpy 2.0
            repodata_json["packages"]["numpy-2.0.0-h12345_0.tar.bz2"]["name"] = "numpy";
            repodata_json["packages"]["numpy-2.0.0-h12345_0.tar.bz2"]["version"] = "2.0.0";
            repodata_json["packages"]["numpy-2.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["numpy-2.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["numpy-2.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";

            // Add somepkg
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["name"] = "somepkg";
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["version"] = "1.0.0";
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";

            // Add testpkg with AND condition: python <3.10 and numpy >=2.0
            // Both conditions should be satisfied (python 3.9 <3.10 AND numpy 2.0 >=2.0)
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["name"] = "testpkg";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["version"] = "1.0.0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["depends"] = nlohmann::json::array(
                {
                    "somepkg; if python <3.10 and numpy >=2.0",  // Should be added because both
                                                                 // conditions are satisfied
                }
            );

            // Write to file
            {
                std::ofstream file(repodata_file);
                file << repodata_json.dump();
            }

            // Parse with Mamba parser
            auto repo1 = db.add_repo_from_repodata_json(
                repodata_file,
                "https://conda.anaconda.org/test/linux-64",
                "test",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOrElseTarBz2,
                libsolv::VerifyPackages::No,
                libsolv::RepodataParser::Mamba
            );

            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 4);  // python, numpy, somepkg, testpkg

            // Check that testpkg has somepkg as a dependency (AND condition satisfied)
            db.for_each_package_in_repo(
                repo1.value(),
                [&](const auto& p)
                {
                    if (p.name == "testpkg")
                    {
                        REQUIRE(p.dependencies.size() == 1);
                        auto ms = specs::MatchSpec::parse(p.dependencies[0]);
                        REQUIRE(ms.has_value());
                        REQUIRE(ms->name().to_string() == "somepkg");
                    }
                }
            );

            // Cleanup
            std::filesystem::remove_all(temp_dir);
        }

        SECTION("Add repo from repodata with conditional dependencies - OR condition")
        {
            // Test that OR conditions are evaluated correctly
            // This is step 5: condition evaluation against pool with OR logic

            auto temp_dir = std::filesystem::temp_directory_path() / "mamba_test_conditional_deps_or";
            std::filesystem::create_directories(temp_dir);
            auto repodata_file = temp_dir / "repodata.json";

            nlohmann::json repodata_json;
            repodata_json["info"]["subdir"] = "linux-64";
            repodata_json["repodata_version"] = 1;

            // Add python 3.9 (matches <3.10 but not >=3.12)
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["name"] = "python";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["version"] = "3.9.0";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["python-3.9.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";

            // Add somepkg
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["name"] = "somepkg";
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["version"] = "1.0.0";
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["somepkg-1.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";

            // Add testpkg with OR condition: python <3.10 or python >=3.12
            // First condition is satisfied (python 3.9 <3.10), so somepkg should be added
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["name"] = "testpkg";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["version"] = "1.0.0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build"] = "h12345_0";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["build_number"] = 0;
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["subdir"] = "linux-64";
            repodata_json["packages"]["testpkg-1.0.0-h12345_0.tar.bz2"]["depends"] = nlohmann::json::array(
                {
                    "somepkg; if python <3.10 or python >=3.12",  // Should be added because first
                                                                  // condition is satisfied
                }
            );

            // Write to file
            {
                std::ofstream file(repodata_file);
                file << repodata_json.dump();
            }

            // Parse with Mamba parser
            auto repo1 = db.add_repo_from_repodata_json(
                repodata_file,
                "https://conda.anaconda.org/test/linux-64",
                "test",
                libsolv::PipAsPythonDependency::No,
                libsolv::PackageTypes::CondaOrElseTarBz2,
                libsolv::VerifyPackages::No,
                libsolv::RepodataParser::Mamba
            );

            REQUIRE(repo1.has_value());
            REQUIRE(repo1->package_count() == 3);  // python, somepkg, testpkg

            // Check that testpkg has somepkg as a dependency (OR condition satisfied)
            db.for_each_package_in_repo(
                repo1.value(),
                [&](const auto& p)
                {
                    if (p.name == "testpkg")
                    {
                        REQUIRE(p.dependencies.size() == 1);
                        auto ms = specs::MatchSpec::parse(p.dependencies[0]);
                        REQUIRE(ms.has_value());
                        REQUIRE(ms->name().to_string() == "somepkg");
                    }
                }
            );

            // Cleanup
            std::filesystem::remove_all(temp_dir);
        }
    }
}
