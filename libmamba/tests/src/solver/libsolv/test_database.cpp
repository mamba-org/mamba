// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <functional>

#include <catch2/catch_all.hpp>

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

    TEST_CASE("Create a database")
    {
        auto db = libsolv::Database({});
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
                    REQUIRE(repo2.package_count() == repo1.package_count();
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
                            CHECK_EQ(
                                p.signatures,
                                R"({"signatures":{"0b7a133184c9c98333923dhfdg86031adc5db1fds54kfga941fe2c94a12fdjg8":{"signature":"0b83c91ddd8b81bbc7a67a586bde4a271bd8f97069c25306870e314f3664ab02083c91ddd8b0dfjsg763jbd0jh14671d960bb303d1eb787307c04c414ediz95a"}}})"
                            );
                        }
                        else if (p.name == "bzip2")
                        {
                            CHECK_EQ(
                                p.signatures,
                                R"({"signatures":{"f7a651f55db194031a6c1240b7a133184c9c98333923dc9319d1fe2c94a1242d":{"signature":"058bf4b5d5cb738736870e314f3664b83c91ddd8b81bbc7a67a875d0454c14671d960a02858e059d154876dab6bde853d763c1a3bd8f97069c25304a2710200d"}}})"
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
                REQUIRE(repo1.has_value());
                REQUIRE(repo1->package_count() == 33);

                db.for_each_package_in_repo(
                    repo1.value(),
                    [&](const auto& p)
                    {
                        if (p.name == "_libgcc_mutex")
                        {
                            REQUIRE(
                                p.signatures.c_str()
                                == doctest::Contains(
                                    R"("signatures":{"0b7a133184c9c98333923dhfdg86031adc5db1fds54kfga941fe2c94a12fdjg8":{"signature":"0b83c91ddd8b81bbc7a67a586bde4a271bd8f97069c25306870e314f3664ab02083c91ddd8b0dfjsg763jbd0jh14671d960bb303d1eb787307c04c414ediz95a"}})"
                                )
                            );
                        }
                        else if (p.name == "bzip2")
                        {
                            REQUIRE(
                                p.signatures.c_str()
                                == doctest::Contains(
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
                        CHECK_EQ(
                            p.package_url,
                            "https://repo.anaconda.com/repo/main/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
                        );
                    }
                    else if (p.name == "bzip2")
                    {
                        CHECK_EQ(
                            p.package_url,
                            "https://repo.anaconda.com/repo/main/linux-64/bzip2-1.0.8-hd590300_5.conda"
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
                        CHECK_EQ(
                            p.package_url,
                            "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
                        );
                    }
                    else if (p.name == "bzip2")
                    {
                        CHECK_EQ(
                            p.package_url,
                            "https://conda.anaconda.org/conda-forge/linux-64/bzip2-1.0.8-hd590300_5.conda"
                        );
                    }
                }
            );
        }
    }
}
