// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"
#include "solv-cpp/solvable.hpp"

using namespace mamba::solv;

TEST_SUITE("solv::ObjSolvable")
{
    TEST_CASE("Create a solvable")
    {
        auto pool = ObjPool();
        auto [repo_id, repo] = pool.add_repo("test-forge");
        const auto [solv_id, solv] = repo.add_solvable();
        CHECK_EQ(solv_id, solv.id());

        SUBCASE("Set name and version")
        {
            solv.set_name("my-package");
            solv.set_version("0.1.1");
            CHECK_EQ(solv.name(), "my-package");
            CHECK_EQ(solv.version(), "0.1.1");

            SUBCASE("Change name version")
            {
                solv.set_name("other-package");
                solv.set_version("0.2.2");
                CHECK_EQ(solv.name(), "other-package");
                CHECK_EQ(solv.version(), "0.2.2");
            }
        }

        SUBCASE("Set attributes")
        {
            solv.set_build_number(33);
            solv.set_build_string("build");
            solv.set_file_name("file.tar.gz");
            solv.set_license("MIT");
            solv.set_md5("6f29ba77e8b03b191c9d667f331bf2a0");
            solv.set_sha256("ecde63af23e0d49c0ece19ec539d873ea408a6f966d3126994c6d33ae1b9d3f7");
            solv.set_noarch(std::string("python"));
            solv.set_size(2345);
            solv.set_timestamp(4110596167);
            solv.set_url("https://conda.anaconda.org/conda-forge/linux-64");
            solv.set_channel("conda-forge");
            solv.set_subdir("linux-64");
            solv.set_artificial(true);

            SUBCASE("Empty without internalize")
            {
                CHECK_EQ(solv.build_number(), 0);
                CHECK_EQ(solv.build_string(), "");
                CHECK_EQ(solv.file_name(), "");
                CHECK_EQ(solv.license(), "");
                CHECK_EQ(solv.md5(), "");
                CHECK_EQ(solv.sha256(), "");
                CHECK_EQ(solv.noarch(), "");
                CHECK_EQ(solv.size(), 0);
                CHECK_EQ(solv.timestamp(), 0);
                CHECK_EQ(solv.url(), "");
                CHECK_EQ(solv.channel(), "");
                CHECK_EQ(solv.subdir(), "");
                CHECK_EQ(solv.artificial(), false);
            }

            SUBCASE("Internalize and get attributes")
            {
                repo.internalize();

                CHECK_EQ(solv.build_string(), "build");
                CHECK_EQ(solv.build_number(), 33);
                CHECK_EQ(solv.build_string(), "build");
                CHECK_EQ(solv.file_name(), "file.tar.gz");
                CHECK_EQ(solv.license(), "MIT");
                CHECK_EQ(solv.md5(), "6f29ba77e8b03b191c9d667f331bf2a0");
                CHECK_EQ(
                    solv.sha256(),
                    "ecde63af23e0d49c0ece19ec539d873ea408a6f966d3126994c6d33ae1b9d3f7"
                );
                CHECK_EQ(solv.noarch(), "python");
                CHECK_EQ(solv.size(), 2345);
                CHECK_EQ(solv.timestamp(), 4110596167);
                CHECK_EQ(solv.url(), "https://conda.anaconda.org/conda-forge/linux-64");
                CHECK_EQ(solv.channel(), "conda-forge");
                CHECK_EQ(solv.subdir(), "linux-64");
                CHECK_EQ(solv.artificial(), true);

                SUBCASE("Override attribute")
                {
                    solv.set_license("GPL");
                    CHECK_EQ(solv.license(), "MIT");
                    repo.internalize();
                    CHECK_EQ(solv.license(), "GPL");
                }
            }
        }

        SUBCASE("Get unset attributes")
        {
            CHECK_EQ(solv.name(), "");
            CHECK_EQ(solv.version(), "");
            CHECK_EQ(solv.build_number(), 0);
            CHECK_EQ(solv.build_string(), "");
            CHECK_EQ(solv.file_name(), "");
            CHECK_EQ(solv.license(), "");
            CHECK_EQ(solv.md5(), "");
            CHECK_EQ(solv.sha256(), "");
            CHECK_EQ(solv.noarch(), "");
            CHECK_EQ(solv.size(), 0);
            CHECK_EQ(solv.timestamp(), 0);
            CHECK_EQ(solv.url(), "");
            CHECK_EQ(solv.channel(), "");
            CHECK_EQ(solv.subdir(), "");
            CHECK_EQ(solv.artificial(), false);
        }

        SUBCASE("Add dependency")
        {
            solv.add_dependency(33);
            CHECK_EQ(solv.dependencies(), ObjQueue{ 33 });

            SUBCASE("Add more dependencies")
            {
                solv.add_dependencies(ObjQueue{ 44, 22 });
                CHECK_EQ(solv.dependencies(), ObjQueue{ 33, 44, 22 });
            }

            SUBCASE("Reset dependencies")
            {
                solv.set_dependencies({});
                CHECK(solv.dependencies().empty());
            }

            SUBCASE("Dependencies with markers")
            {
                solv.add_dependency(34);
                solv.add_dependency(11, SOLVABLE_PREREQMARKER);
                solv.add_dependency(35);

                CHECK_EQ(solv.dependencies(-1), ObjQueue{ 33, 34 });
                CHECK_EQ(solv.dependencies(0), ObjQueue{ 33, 34, SOLVABLE_PREREQMARKER, 11, 35 });
                CHECK_EQ(solv.dependencies(1), ObjQueue{ 11, 35 });
                CHECK_EQ(solv.dependencies(SOLVABLE_PREREQMARKER), ObjQueue{ 11, 35 });
            }
        }

        SUBCASE("Add provide")
        {
            solv.add_provide(33);
            CHECK_EQ(solv.provides(), ObjQueue{ 33 });

            SUBCASE("Add self provide")
            {
                solv.add_self_provide();
                CHECK_EQ(solv.provides().size(), 2);
            }

            SUBCASE("Add more provides")
            {
                solv.add_provides(ObjQueue{ 44, 22 });
                CHECK_EQ(solv.provides(), ObjQueue{ 33, 44, 22 });
            }

            SUBCASE("Reset provides")
            {
                solv.set_provides({});
                CHECK(solv.provides().empty());
            }
        }

        SUBCASE("Add constraint")
        {
            solv.add_constraint(33);

            SUBCASE("Internalize and get constraint")
            {
                repo.internalize();
                CHECK_EQ(solv.constraints(), ObjQueue{ 33 });

                SUBCASE("Fail to add more constraint")
                {
                    solv.add_constraint(44);
                    CHECK_EQ(solv.constraints(), ObjQueue{ 33 });

                    SUBCASE("Override when internalizing again")
                    {
                        repo.internalize();
                        CHECK_EQ(solv.constraints(), ObjQueue{ 44 });
                    }
                }

                SUBCASE("Fail to set constraints")
                {
                    solv.set_constraints({ 22 });
                    CHECK_EQ(solv.constraints(), ObjQueue{ 33 });

                    SUBCASE("Override when internalizing again")
                    {
                        repo.internalize();
                        CHECK_EQ(solv.constraints(), ObjQueue{ 22 });
                    }
                }
            }

            SUBCASE("Add more constraints")
            {
                solv.add_constraints(ObjQueue{ 44, 22 });
                repo.internalize();
                CHECK_EQ(solv.constraints(), ObjQueue{ 33, 44, 22 });
            }

            SUBCASE("Reset constraints")
            {
                solv.set_constraints({});
                repo.internalize();
                CHECK(solv.constraints().empty());
            }
        }

        SUBCASE("Track feature")
        {
            const StringId feat1_id = solv.add_track_feature("feature1");

            SUBCASE("Internalize and get tracked features")
            {
                repo.internalize();
                CHECK_EQ(solv.track_features(), ObjQueue{ feat1_id });

                SUBCASE("Fail to track more features")
                {
                    const StringId feat2_id = solv.add_track_feature("feature2");
                    CHECK_EQ(solv.track_features(), ObjQueue{ feat1_id });

                    SUBCASE("Override when internalizing again")
                    {
                        repo.internalize();
                        CHECK_EQ(solv.track_features(), ObjQueue{ feat2_id });
                    }
                }

                SUBCASE("Fail to set tracked features")
                {
                    solv.set_track_features({ 22 });
                    CHECK_EQ(solv.track_features(), ObjQueue{ feat1_id });

                    SUBCASE("Override when internalizing again")
                    {
                        repo.internalize();
                        CHECK_EQ(solv.track_features(), ObjQueue{ 22 });
                    }
                }
            }

            SUBCASE("Track more features")
            {
                solv.add_track_features(ObjQueue{ 44, 11 });
                repo.internalize();
                CHECK_EQ(solv.track_features(), ObjQueue{ feat1_id, 44, 11 });
            }

            SUBCASE("Reset tracked features")
            {
                solv.set_track_features({});
                repo.internalize();
                CHECK(solv.track_features().empty());
            }
        }
    }
}
