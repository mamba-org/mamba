// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>
#include <unordered_map>

#include <catch2/catch_all.hpp>

#include "mamba/util/weakening_map.hpp"


using namespace mamba::util;

TEST_CASE("DecreaseWeakener")
{
    struct DecreaseWeakener
    {
        [[nodiscard]] auto make_first_key(int key) const -> int
        {
            return key + 1;
        }

        [[nodiscard]] auto weaken_key(int key) const -> std::optional<int>
        {
            if (key > 1)
            {
                return { key - 1 };
            }
            return std::nullopt;
        }
    };

    using test_map = weakening_map<std::unordered_map<int, int>, DecreaseWeakener>;

    SECTION("empty")
    {
        auto map = test_map();

        REQUIRE_FALSE(map.contains_weaken(1));
        REQUIRE_FALSE(map.contains_weaken(0));
    }

    SECTION("key match")
    {
        auto map = test_map({ { 1, 10 }, { 4, 40 } });

        REQUIRE_FALSE(map.contains_weaken(-1));

        REQUIRE(map.at_weaken(4) == 40);  // Exact match
        REQUIRE(map.at_weaken(0) == 10);  // First key match
        REQUIRE(map.at_weaken(7) == 40);  // Weaken key
    }
}
