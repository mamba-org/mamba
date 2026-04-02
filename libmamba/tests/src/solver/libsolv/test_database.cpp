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
    }

    /**
     * Tests for defaulted_keys preservation through Database round-trip
     *
     * PURPOSE: Verify that PackageInfo.defaulted_keys survives the round-trip through
     * the libsolv database (add_repo_from_packages -> for_each_package_in_repo).
     *
     * MOTIVATION: Issue #4095 - URL-derived packages lose their defaulted_keys when
     * going through the solver because set_solvable() and make_package_info() didn't
     * preserve this field. This test ensures the fix works correctly.
     *
     * SEMANTICS of defaulted_keys:
     * - Empty: INVALID (missing "_initialized" sentinel)
     * - ["_initialized"]: Properly initialized, trust all fields
     * - ["_initialized", "field1", ...]: Properly initialized, these fields have stub values
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("Database preserves defaulted_keys")
    {
        auto db = libsolv::Database({}, { libsolv::MatchSpecParser::Mamba });

        SECTION("defaulted_keys survives round-trip through database")
        {
            // PURPOSE: Verify that a full defaulted_keys list (like from from_url())
            // is preserved when a package goes through the database.
            auto pkg = specs::PackageInfo();
            pkg.name = "url-derived-pkg";
            pkg.version = "1.0";
            pkg.build_string = "h123_0";
            pkg.channel = "conda-forge";
            // URL-derived packages have a full list of stub fields
            pkg.defaulted_keys = { "_initialized", "build_number",   "license", "timestamp", "md5",
                                   "sha256",       "track_features", "depends", "constrains" };

            auto repo = db.add_repo_from_packages(std::array{ pkg }, "test-repo");

            std::size_t count = 0;
            db.for_each_package_in_repo(
                repo,
                [&](const auto& p)
                {
                    count++;
                    REQUIRE(p.name == "url-derived-pkg");
                    // This is the critical assertion - defaulted_keys must be preserved
                    REQUIRE(p.defaulted_keys == pkg.defaulted_keys);
                }
            );
            REQUIRE(count == 1);
        }

        SECTION("empty defaulted_keys becomes _initialized (backward compat fallback)")
        {
            // PURPOSE: Verify backward compatibility with old code/cache that has
            // empty defaulted_keys. The fallback should convert empty to ["_initialized"].
            auto pkg = specs::PackageInfo();
            pkg.name = "legacy-pkg";
            pkg.version = "2.0";
            pkg.build_string = "h456_0";
            pkg.channel = "conda-forge";
            pkg.defaulted_keys = {};  // Empty = invalid, should be converted

            auto repo = db.add_repo_from_packages(std::array{ pkg }, "test-repo");

            db.for_each_package_in_repo(
                repo,
                [&](const auto& p)
                {
                    REQUIRE(p.name == "legacy-pkg");
                    // Fallback converts empty to ["_initialized"]
                    REQUIRE(p.defaulted_keys.size() == 1);
                    REQUIRE(p.defaulted_keys[0] == "_initialized");
                }
            );
        }

        SECTION("_initialized only is preserved (channel-derived)")
        {
            // PURPOSE: Verify that packages with only _initialized (like channel
            // repodata packages) preserve this exact state.
            auto pkg = specs::PackageInfo();
            pkg.name = "channel-pkg";
            pkg.version = "3.0";
            pkg.build_string = "0";
            pkg.channel = "conda-forge";
            // Channel-derived packages have only _initialized (trust all fields)
            pkg.defaulted_keys = { "_initialized" };

            auto repo = db.add_repo_from_packages(std::array{ pkg }, "test-repo");

            db.for_each_package_in_repo(
                repo,
                [&](const auto& p)
                {
                    REQUIRE(p.name == "channel-pkg");
                    REQUIRE(p.defaulted_keys.size() == 1);
                    REQUIRE(p.defaulted_keys[0] == "_initialized");
                }
            );
        }
    }

    /**
     * Tests for channel-derived packages from repodata JSON
     *
     * PURPOSE: Verify that packages loaded from channel repodata JSON have
     * ["_initialized"] set, indicating they have authoritative metadata.
     *
     * MOTIVATION: Issue #4095 - Channel repodata packages must have the
     * "_initialized" sentinel so write_repodata_record() knows they're valid.
     * Without this, all channel installs would fail with "missing _initialized".
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("Channel-derived packages from repodata have _initialized")
    {
        auto db = libsolv::Database({}, { libsolv::MatchSpecParser::Mamba });

        const auto repodata = mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json";
        auto repo = db.add_repo_from_repodata_json(
            repodata,
            "https://conda.anaconda.org/conda-forge/linux-64",
            "conda-forge",
            libsolv::PipAsPythonDependency::No
        );
        REQUIRE(repo.has_value());
        REQUIRE(repo->package_count() > 0);

        // Channel repodata packages should have ["_initialized"] because all their
        // metadata is authoritative (no stub values). This is critical for
        // write_repodata_record() to work correctly. See GitHub issue #4095.
        std::size_t count = 0;
        db.for_each_package_in_repo(
            repo.value(),
            [&](const auto& pkg)
            {
                count++;
                REQUIRE(pkg.defaulted_keys.size() == 1);
                REQUIRE(pkg.defaulted_keys[0] == "_initialized");
            }
        );
        REQUIRE(count == repo->package_count());
    }
}
