// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"
#include "solv-cpp/solvable.hpp"

using namespace solv;

TEST_CASE("Create a solvable")
{
    auto pool = ObjPool();
    auto [repo_id, repo] = pool.add_repo("test-forge");
    const auto [solv_id, solv] = repo.add_solvable();
    REQUIRE(solv_id == solv.id());

    SECTION("Set name and version")
    {
        solv.set_name("my-package");
        solv.set_version("0.1.1");
        REQUIRE(solv.name() == "my-package");
        REQUIRE(solv.version() == "0.1.1");

        SECTION("Change name version")
        {
            solv.set_name("other-package");
            solv.set_version("0.2.2");
            REQUIRE(solv.name() == "other-package");
            REQUIRE(solv.version() == "0.2.2");
        }
    }

    SECTION("Set attributes")
    {
        solv.set_build_number(33);
        solv.set_build_string("build");
        solv.set_file_name("file.tar.gz");
        solv.set_license("MIT");
        solv.set_md5("6f29ba77e8b03b191c9d667f331bf2a0");
        solv.set_sha256("ecde63af23e0d49c0ece19ec539d873ea408a6f966d3126994c6d33ae1b9d3f7");
        solv.set_signatures(
            R"("signatures": { "some_file.tar.bz2": { "a133184c9c7a651f55db194031a6c1240b798333923dc9319d1fe2c94a1242d": { "signature": "7a67a875d0454c14671d960a02858e059d154876dab6b3873304a27102063c9c25"}}})"
        );
        solv.set_noarch(std::string("python"));
        solv.set_size(2345);
        solv.set_timestamp(4110596167);
        solv.set_url("https://conda.anaconda.org/conda-forge/linux-64");
        solv.set_channel("conda-forge");
        solv.set_platform("linux-64");
        solv.set_type(SolvableType::Virtualpackage);

        SECTION("Empty without internalize")
        {
            REQUIRE(solv.build_number() == 0);
            REQUIRE(solv.build_string() == "");
            REQUIRE(solv.file_name() == "");
            REQUIRE(solv.license() == "");
            REQUIRE(solv.md5() == "");
            REQUIRE(solv.sha256() == "");
            REQUIRE(solv.signatures() == "");
            REQUIRE(solv.noarch() == "");
            REQUIRE(solv.size() == 0);
            REQUIRE(solv.timestamp() == 0);
            REQUIRE(solv.url() == "");
            REQUIRE(solv.channel() == "");
            REQUIRE(solv.platform() == "");
            REQUIRE(solv.type() == SolvableType::Package);
        }

        SECTION("Internalize and get attributes")
        {
            repo.internalize();

            REQUIRE(solv.build_string() == "build");
            REQUIRE(solv.build_number() == 33);
            REQUIRE(solv.build_string() == "build");
            REQUIRE(solv.file_name() == "file.tar.gz");
            REQUIRE(solv.license() == "MIT");
            REQUIRE(solv.md5() == "6f29ba77e8b03b191c9d667f331bf2a0");
            REQUIRE(
                solv.sha256() == "ecde63af23e0d49c0ece19ec539d873ea408a6f966d3126994c6d33ae1b9d3f7"
            );
            REQUIRE(
                solv.signatures()
                == R"("signatures": { "some_file.tar.bz2": { "a133184c9c7a651f55db194031a6c1240b798333923dc9319d1fe2c94a1242d": { "signature": "7a67a875d0454c14671d960a02858e059d154876dab6b3873304a27102063c9c25"}}})"
            );
            REQUIRE(solv.noarch() == "python");
            REQUIRE(solv.size() == 2345);
            REQUIRE(solv.timestamp() == 4110596167);
            REQUIRE(solv.url() == "https://conda.anaconda.org/conda-forge/linux-64");
            REQUIRE(solv.channel() == "conda-forge");
            REQUIRE(solv.platform() == "linux-64");
            REQUIRE(solv.type() == SolvableType::Virtualpackage);

            SECTION("Override attribute")
            {
                solv.set_license("GPL");
                REQUIRE(solv.license() == "MIT");
                repo.internalize();
                REQUIRE(solv.license() == "GPL");
            }
        }
    }

    SECTION("Get unset attributes")
    {
        REQUIRE(solv.name() == "");
        REQUIRE(solv.version() == "");
        REQUIRE(solv.build_number() == 0);
        REQUIRE(solv.build_string() == "");
        REQUIRE(solv.file_name() == "");
        REQUIRE(solv.license() == "");
        REQUIRE(solv.md5() == "");
        REQUIRE(solv.sha256() == "");
        REQUIRE(solv.signatures() == "");
        REQUIRE(solv.noarch() == "");
        REQUIRE(solv.size() == 0);
        REQUIRE(solv.timestamp() == 0);
        REQUIRE(solv.url() == "");
        REQUIRE(solv.channel() == "");
        REQUIRE(solv.platform() == "");
        REQUIRE(solv.type() == SolvableType::Package);
    }

    SECTION("Add dependency")
    {
        solv.add_dependency(33);
        REQUIRE(solv.dependencies() == ObjQueue{ 33 });

        SECTION("Add more dependencies")
        {
            solv.add_dependencies(ObjQueue{ 44, 22 });
            REQUIRE(solv.dependencies() == ObjQueue{ 33, 44, 22 });
        }

        SECTION("Reset dependencies")
        {
            solv.set_dependencies({});
            REQUIRE(solv.dependencies().empty());
        }

        SECTION("Dependencies with markers")
        {
            solv.add_dependency(34);
            solv.add_dependency(11, SOLVABLE_PREREQMARKER);
            solv.add_dependency(35);

            REQUIRE(solv.dependencies(-1) == ObjQueue{ 33, 34 });
            REQUIRE(solv.dependencies(0) == ObjQueue{ 33, 34, SOLVABLE_PREREQMARKER, 11, 35 });
            REQUIRE(solv.dependencies(1) == ObjQueue{ 11, 35 });
            REQUIRE(solv.dependencies(SOLVABLE_PREREQMARKER) == ObjQueue{ 11, 35 });
        }
    }

    SECTION("Add provide")
    {
        solv.add_provide(33);
        REQUIRE(solv.provides() == ObjQueue{ 33 });

        SECTION("Add self provide")
        {
            solv.add_self_provide();
            REQUIRE(solv.provides().size() == 2);
        }

        SECTION("Add more provides")
        {
            solv.add_provides(ObjQueue{ 44, 22 });
            REQUIRE(solv.provides() == ObjQueue{ 33, 44, 22 });
        }

        SECTION("Reset provides")
        {
            solv.set_provides({});
            REQUIRE(solv.provides().empty());
        }
    }

    SECTION("Add constraint")
    {
        solv.add_constraint(33);

        SECTION("Internalize and get constraint")
        {
            repo.internalize();
            REQUIRE(solv.constraints() == ObjQueue{ 33 });

            SECTION("Fail to add more constraint")
            {
                solv.add_constraint(44);
                REQUIRE(solv.constraints() == ObjQueue{ 33 });

                SECTION("Override when internalizing again")
                {
                    repo.internalize();
                    REQUIRE(solv.constraints() == ObjQueue{ 44 });
                }
            }

            SECTION("Fail to set constraints")
            {
                solv.set_constraints({ 22 });
                REQUIRE(solv.constraints() == ObjQueue{ 33 });

                SECTION("Override when internalizing again")
                {
                    repo.internalize();
                    REQUIRE(solv.constraints() == ObjQueue{ 22 });
                }
            }
        }

        SECTION("Add more constraints")
        {
            solv.add_constraints(ObjQueue{ 44, 22 });
            repo.internalize();
            REQUIRE(solv.constraints() == ObjQueue{ 33, 44, 22 });
        }

        SECTION("Reset constraints")
        {
            solv.set_constraints({});
            repo.internalize();
            REQUIRE(solv.constraints().empty());
        }
    }

    SECTION("Track feature")
    {
        const StringId feat1_id = solv.add_track_feature("feature1");

        SECTION("Internalize and get tracked features")
        {
            repo.internalize();
            REQUIRE(solv.track_features() == ObjQueue{ feat1_id });

            SECTION("Fail to track more features")
            {
                const StringId feat2_id = solv.add_track_feature("feature2");
                REQUIRE(solv.track_features() == ObjQueue{ feat1_id });

                SECTION("Override when internalizing again")
                {
                    repo.internalize();
                    REQUIRE(solv.track_features() == ObjQueue{ feat2_id });
                }
            }

            SECTION("Fail to set tracked features")
            {
                solv.set_track_features({ 22 });
                REQUIRE(solv.track_features() == ObjQueue{ feat1_id });

                SECTION("Override when internalizing again")
                {
                    repo.internalize();
                    REQUIRE(solv.track_features() == ObjQueue{ 22 });
                }
            }
        }

        SECTION("Track more features")
        {
            solv.add_track_features(ObjQueue{ 44, 11 });
            repo.internalize();
            REQUIRE(solv.track_features() == ObjQueue{ feat1_id, 44, 11 });
        }

        SECTION("Reset tracked features")
        {
            solv.set_track_features({});
            repo.internalize();
            REQUIRE(solv.track_features().empty());
        }
    }
}
