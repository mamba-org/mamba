// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>
#include <solv/pool.h>

#include "solv-cpp/pool.hpp"

using namespace mamba::solv;

TEST_SUITE("ObjPool")
{
    TEST_CASE("Construct a pool")
    {
        auto pool = ObjPool();

        SUBCASE("Add strings")
        {
            const auto id_hello = pool.add_string("Hello");
            const auto maybe_id_hello = pool.find_string("Hello");
            REQUIRE(maybe_id_hello.has_value());
            CHECK_EQ(maybe_id_hello.value(), id_hello);
            CHECK_EQ(pool.get_string(id_hello), "Hello");

            SUBCASE("Add another string")
            {
                const auto id_world = pool.add_string("World");
                CHECK_NE(id_world, id_hello);
                const auto maybe_id_world = pool.find_string("World");
                REQUIRE(maybe_id_world.has_value());
                CHECK_EQ(maybe_id_world.value(), id_world);
                CHECK_EQ(pool.get_string(id_world), "World");

                SUBCASE("Add the same one again")
                {
                    const auto id_world_again = pool.add_string("World");
                    CHECK_EQ(id_world_again, id_world);
                }
            }

            SUBCASE("Find non-existant string")
            {
                CHECK_FALSE(pool.find_string("Bar").has_value());
            }
        }

        SUBCASE("Add dependencies")
        {
            const auto id_name = pool.add_string("mamba");
            const auto id_version_1 = pool.add_string("1.0.0");

            const auto id_rel = pool.add_dependency(id_name, REL_GT, id_version_1);
            const auto maybe_id_rel = pool.find_dependency(id_name, REL_GT, id_version_1);
            REQUIRE(maybe_id_rel.has_value());
            CHECK_EQ(maybe_id_rel.value(), id_rel);
            CHECK_EQ(pool.get_dependency_name(id_rel), "mamba");
            CHECK_EQ(pool.get_dependency_relation(id_rel), " > ");
            CHECK_EQ(pool.get_dependency_version(id_rel), "1.0.0");
            CHECK_EQ(pool.dependency_to_string(id_rel), "mamba > 1.0.0");
        }
    }
}
