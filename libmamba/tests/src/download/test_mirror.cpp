// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <typeinfo>

#include <doctest/doctest.h>

#include "../src/download/mirror_impl.hpp"

namespace mamba::download
{
    namespace utils
    {
        std::pair<std::string, std::string> split_path_tag(const std::string& path);

        TEST_SUITE("split_path_tag")
        {
            TEST_CASE("tar_bz2_extension")
            {
                auto [split_path, split_tag] = split_path_tag("xtensor-0.23.10-h2acdbc0_0.tar.bz2");
                CHECK_EQ(split_path, "xtensor");
                CHECK_EQ(split_tag, "0.23.10-h2acdbc0-0");
            }

            TEST_CASE("multiple_parts")
            {
                auto [split_path, split_tag] = split_path_tag("x-tensor-10.23.10-h2acdbc0_0.tar.bz2");
                CHECK_EQ(split_path, "x-tensor");
                CHECK_EQ(split_tag, "10.23.10-h2acdbc0-0");
            }

            TEST_CASE("more_multiple_parts")
            {
                auto [split_path, split_tag] = split_path_tag("x-tens-or-10.23.10-h2acdbc0_0.tar.bz2");
                CHECK_EQ(split_path, "x-tens-or");
                CHECK_EQ(split_tag, "10.23.10-h2acdbc0-0");
            }

            TEST_CASE("json_extension")
            {
                auto [split_path, split_tag] = split_path_tag("xtensor-0.23.10-h2acdbc0_0.json");
                CHECK_EQ(split_path, "xtensor-0.23.10-h2acdbc0_0.json");
                CHECK_EQ(split_tag, "latest");
            }

            TEST_CASE("not_enough_parts")
            {
                CHECK_THROWS_AS(split_path_tag("xtensor.tar.bz2"), std::runtime_error);
            }
        }
    }

    TEST_SUITE("mirrors")
    {
        TEST_CASE("PassThroughMirror")
        {
            std::unique_ptr<Mirror> mir = make_mirror("");
            auto& ref = *mir;
            CHECK_EQ(typeid(ref), typeid(PassThroughMirror));

            Mirror::request_generator_list req_gen = mir->get_request_generators("", "");
            CHECK_EQ(req_gen.size(), 1);

            Request req_repodata("some_request_name", MirrorName("mirror_name"), "linux-64/repodata.json");
            MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

            CHECK_EQ(mir_req.name, "some_request_name");
            CHECK_EQ(mir_req.url, "linux-64/repodata.json");
        }

        TEST_CASE("HTTPMirror")
        {
            SUBCASE("https")
            {
                std::unique_ptr<Mirror> mir = make_mirror("https://conda.anaconda.org/conda-forge");
                auto& ref = *mir;
                CHECK_EQ(typeid(ref), typeid(HTTPMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators("", "");
                CHECK_EQ(req_gen.size(), 1);

                Request req_repodata(
                    "repodata_request",
                    MirrorName("mirror_name"),
                    "linux-64/repodata.json"
                );
                MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

                CHECK_EQ(mir_req.name, "repodata_request");
                CHECK_EQ(mir_req.url, "https://conda.anaconda.org/conda-forge/linux-64/repodata.json");
            }

            SUBCASE("http")
            {
                std::unique_ptr<Mirror> mir = make_mirror("http://conda.anaconda.org/conda-forge");
                auto& ref = *mir;
                CHECK_EQ(typeid(ref), typeid(HTTPMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators("", "");
                CHECK_EQ(req_gen.size(), 1);

                Request req_repodata(
                    "repodata_request",
                    MirrorName("mirror_name"),
                    "linux-64/repodata.json"
                );
                MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

                CHECK_EQ(mir_req.name, "repodata_request");
                CHECK_EQ(mir_req.url, "http://conda.anaconda.org/conda-forge/linux-64/repodata.json");
            }

            SUBCASE("file")
            {
                std::unique_ptr<Mirror> mir = make_mirror("file://channel_path");
                auto& ref = *mir;
                CHECK_EQ(typeid(ref), typeid(HTTPMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators("", "");
                CHECK_EQ(req_gen.size(), 1);

                Request req_repodata(
                    "repodata_request",
                    MirrorName("mirror_name"),
                    "linux-64/repodata.json"
                );
                MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

                CHECK_EQ(mir_req.name, "repodata_request");
                CHECK_EQ(mir_req.url, "file://channel_path/linux-64/repodata.json");
            }
        }

        TEST_CASE("OCIMirror")
        {
            SUBCASE("Request repodata.json")
            {
                std::unique_ptr<Mirror> mir = make_mirror("oci://ghcr.io/channel-mirrors/conda-forge");
                auto& ref = *mir;
                CHECK_EQ(typeid(ref), typeid(OCIMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators(
                    "linux-64/repodata.json",
                    ""
                );
                CHECK_EQ(req_gen.size(), 3);

                Request req_repodata(
                    "repodata_request",
                    MirrorName("mirror_name"),
                    "linux-64/repodata.json"
                );
                MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

                CHECK_EQ(mir_req.name, "repodata_request");
                CHECK_EQ(
                    mir_req.url,
                    "https://ghcr.io/token?scope=repository:channel-mirrors/conda-forge/linux-64/repodata.json:pull"
                );

                // Empty token leads to throwing an exception
                CHECK_THROWS_AS(req_gen[1](req_repodata, nullptr), std::invalid_argument);
                CHECK_THROWS_AS(req_gen[2](req_repodata, nullptr), std::invalid_argument);
            }

            SUBCASE("Request spec with sha")
            {
                std::unique_ptr<Mirror> mir = make_mirror("oci://ghcr.io/channel-mirrors/conda-forge");
                auto& ref = *mir;
                CHECK_EQ(typeid(ref), typeid(OCIMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators(
                    "linux-64/pandoc-3.2-ha770c72_0.conda",
                    "418348076c1a39170efb0bdc8a584ddd11e9ed0ff58ccd905488d3f165ca98ba"
                );
                CHECK_EQ(req_gen.size(), 2);

                Request req_spec(
                    "pandoc_request",
                    MirrorName("mirror_name"),
                    "linux-64/pandoc-3.2-ha770c72_0.conda"
                );
                MirrorRequest mir_req = req_gen[0](req_spec, nullptr);

                CHECK_EQ(mir_req.name, "pandoc_request");
                CHECK_EQ(
                    mir_req.url,
                    "https://ghcr.io/token?scope=repository:channel-mirrors/conda-forge/linux-64/pandoc:pull"
                );

                // Empty token leads to throwing an exception
                CHECK_THROWS_AS(req_gen[1](req_spec, nullptr), std::invalid_argument);
            }
        }

        TEST_CASE("nullptr")
        {
            std::unique_ptr<Mirror> mir = make_mirror("ghcr.io/channel-mirrors/conda-forge");
            CHECK_EQ(mir, nullptr);
        }
    }
}
