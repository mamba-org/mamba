// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/core/mirror.hpp"

namespace mamba
{
    TEST_SUITE("mirror")
    {
        TEST_CASE("split_path_tag")
        {
            SUBCASE("tar_bz2_extension")
            {
                auto [split_path, split_tag] = split_path_tag("xtensor-0.23.10-h2acdbc0_0.tar.bz2");
                CHECK_EQ(split_path, "xtensor");
                CHECK_EQ(split_tag, "0.23.10-h2acdbc0-0");
            }

            SUBCASE("multiple_parts")
            {
                auto [split_path, split_tag] = split_path_tag("x-tensor-10.23.10-h2acdbc0_0.tar.bz2");
                CHECK_EQ(split_path, "x-tensor");
                CHECK_EQ(split_tag, "10.23.10-h2acdbc0-0");
            }

            SUBCASE("more_multiple_parts")
            {
                auto [split_path, split_tag] = split_path_tag("x-tens-or-10.23.10-h2acdbc0_0.tar.bz2");
                CHECK_EQ(split_path, "x-tens-or");
                CHECK_EQ(split_tag, "10.23.10-h2acdbc0-0");
            }

            SUBCASE("json_extension")
            {
                auto [split_path, split_tag] = split_path_tag("xtensor-0.23.10-h2acdbc0_0.json");
                CHECK_EQ(split_path, "xtensor-0.23.10-h2acdbc0_0.json");
                CHECK_EQ(split_tag, "latest");
            }

            SUBCASE("not_enough_parts")
            {
                CHECK_THROWS_AS(split_path_tag("xtensor.tar.bz2"), std::runtime_error);
            }
        }
    }
}
