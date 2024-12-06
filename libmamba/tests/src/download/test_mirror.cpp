// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <typeinfo>

#include <catch2/catch_all.hpp>

#include "../src/download/mirror_impl.hpp"

namespace mamba::download
{
    namespace utils
    {
        std::pair<std::string, std::string> split_path_tag(const std::string& path);

        namespace
        {
            TEST_CASE("tar_bz2_extension")
            {
                auto [split_path, split_tag] = split_path_tag("xtensor-0.23.10-h2acdbc0_0.tar.bz2");
                REQUIRE(split_path == "xtensor");
                REQUIRE(split_tag == "0.23.10-h2acdbc0-0");
            }

            TEST_CASE("multiple_parts")
            {
                auto [split_path, split_tag] = split_path_tag("x-tensor-10.23.10-h2acdbc0_0.tar.bz2");
                REQUIRE(split_path == "x-tensor");
                REQUIRE(split_tag == "10.23.10-h2acdbc0-0");
            }

            TEST_CASE("more_multiple_parts")
            {
                auto [split_path, split_tag] = split_path_tag("x-tens-or-10.23.10-h2acdbc0_0.tar.bz2");
                REQUIRE(split_path == "x-tens-or");
                REQUIRE(split_tag == "10.23.10-h2acdbc0-0");
            }

            TEST_CASE("json_extension")
            {
                auto [split_path, split_tag] = split_path_tag("xtensor-0.23.10-h2acdbc0_0.json");
                REQUIRE(split_path == "xtensor-0.23.10-h2acdbc0_0.json");
                REQUIRE(split_tag == "latest");
            }

            TEST_CASE("not_enough_parts")
            {
                REQUIRE_THROWS_AS(split_path_tag("xtensor.tar.bz2"), std::runtime_error);
            }
        }
    }

    namespace
    {
        TEST_CASE("PassThroughMirror")
        {
            std::unique_ptr<Mirror> mir = make_mirror("");
            // `mir_ref` is used here to provide an explicit expression to `typeid`
            // and avoid expression with side effects evaluation warning
            auto& mir_ref = *mir;
            REQUIRE(typeid(mir_ref) == typeid(PassThroughMirror));

            Mirror::request_generator_list req_gen = mir->get_request_generators("", "");
            REQUIRE(req_gen.size() == 1);

            Request req_repodata("some_request_name", MirrorName("mirror_name"), "linux-64/repodata.json");
            MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

            REQUIRE(mir_req.name == "some_request_name");
            REQUIRE(mir_req.url == "linux-64/repodata.json");
        }

        TEST_CASE("HTTPMirror")
        {
            SECTION("https")
            {
                std::unique_ptr<Mirror> mir = make_mirror("https://conda.anaconda.org/conda-forge");
                // `mir_ref` is used here to provide an explicit expression to `typeid`
                // and avoid expression with side effects evaluation warning
                auto& mir_ref = *mir;
                REQUIRE(typeid(mir_ref) == typeid(HTTPMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators("", "");
                REQUIRE(req_gen.size() == 1);

                Request req_repodata(
                    "repodata_request",
                    MirrorName("mirror_name"),
                    "linux-64/repodata.json"
                );
                MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

                REQUIRE(mir_req.name == "repodata_request");
                REQUIRE(mir_req.url == "https://conda.anaconda.org/conda-forge/linux-64/repodata.json");
            }

            SECTION("http")
            {
                std::unique_ptr<Mirror> mir = make_mirror("http://conda.anaconda.org/conda-forge");
                // `mir_ref` is used here to provide an explicit expression to `typeid`
                // and avoid expression with side effects evaluation warning
                auto& mir_ref = *mir;
                REQUIRE(typeid(mir_ref) == typeid(HTTPMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators("", "");
                REQUIRE(req_gen.size() == 1);

                Request req_repodata(
                    "repodata_request",
                    MirrorName("mirror_name"),
                    "linux-64/repodata.json"
                );
                MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

                REQUIRE(mir_req.name == "repodata_request");
                REQUIRE(mir_req.url == "http://conda.anaconda.org/conda-forge/linux-64/repodata.json");
            }

            SECTION("file")
            {
                std::unique_ptr<Mirror> mir = make_mirror("file://channel_path");
                // `mir_ref` is used here to provide an explicit expression to `typeid`
                // and avoid expression with side effects evaluation warning
                auto& mir_ref = *mir;
                REQUIRE(typeid(mir_ref) == typeid(HTTPMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators("", "");
                REQUIRE(req_gen.size() == 1);

                Request req_repodata(
                    "repodata_request",
                    MirrorName("mirror_name"),
                    "linux-64/repodata.json"
                );
                MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

                REQUIRE(mir_req.name == "repodata_request");
                REQUIRE(mir_req.url == "file://channel_path/linux-64/repodata.json");
            }
        }

        TEST_CASE("OCIMirror")
        {
            SECTION("Request repodata.json")
            {
                std::unique_ptr<Mirror> mir = make_mirror("oci://ghcr.io/channel-mirrors/conda-forge");
                // `mir_ref` is used here to provide an explicit expression to `typeid`
                // and avoid expression with side effects evaluation warning
                auto& mir_ref = *mir;
                REQUIRE(typeid(mir_ref) == typeid(OCIMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators(
                    "linux-64/repodata.json",
                    ""
                );
                REQUIRE(req_gen.size() == 3);

                Request req_repodata(
                    "repodata_request",
                    MirrorName("mirror_name"),
                    "linux-64/repodata.json"
                );
                MirrorRequest mir_req = req_gen[0](req_repodata, nullptr);

                REQUIRE(mir_req.name == "repodata_request");
                REQUIRE(
                    mir_req.url
                    == "https://ghcr.io/token?scope=repository:channel-mirrors/conda-forge/linux-64/repodata.json:pull"
                );

                // Empty token leads to throwing an exception
                REQUIRE_THROWS_AS(req_gen[1](req_repodata, nullptr), std::invalid_argument);
                REQUIRE_THROWS_AS(req_gen[2](req_repodata, nullptr), std::invalid_argument);
            }

            SECTION("Request spec with sha")
            {
                std::unique_ptr<Mirror> mir = make_mirror("oci://ghcr.io/channel-mirrors/conda-forge");
                // `mir_ref` is used here to provide an explicit expression to `typeid`
                // and avoid expression with side effects evaluation warning
                auto& mir_ref = *mir;
                REQUIRE(typeid(mir_ref) == typeid(OCIMirror));

                Mirror::request_generator_list req_gen = mir->get_request_generators(
                    "linux-64/pandoc-3.2-ha770c72_0.conda",
                    "418348076c1a39170efb0bdc8a584ddd11e9ed0ff58ccd905488d3f165ca98ba"
                );
                REQUIRE(req_gen.size() == 2);

                Request req_spec(
                    "pandoc_request",
                    MirrorName("mirror_name"),
                    "linux-64/pandoc-3.2-ha770c72_0.conda"
                );
                MirrorRequest mir_req = req_gen[0](req_spec, nullptr);

                REQUIRE(mir_req.name == "pandoc_request");
                REQUIRE(
                    mir_req.url
                    == "https://ghcr.io/token?scope=repository:channel-mirrors/conda-forge/linux-64/pandoc:pull"
                );

                // Empty token leads to throwing an exception
                REQUIRE_THROWS_AS(req_gen[1](req_spec, nullptr), std::invalid_argument);
            }
        }

        TEST_CASE("nullptr")
        {
            std::unique_ptr<Mirror> mir = make_mirror("ghcr.io/channel-mirrors/conda-forge");
            REQUIRE(mir == nullptr);
        }
    }
}
