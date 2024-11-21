// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/util/heap_optional.hpp"

using namespace mamba::util;

namespace
{
    TEST_CASE("heap_optional")
    {
        SECTION("Without value")
        {
            auto opt = heap_optional<int>();
            REQUIRE_FALSE(opt.has_value());
            REQUIRE_FALSE(opt);
            CHECK_EQ(opt.get(), nullptr);

            SECTION("Emplace data")
            {
                opt.emplace(3);
                REQUIRE(opt.has_value());
                REQUIRE(opt);
                REQUIRE_NE(opt.get(), nullptr);
                CHECK_EQ(*opt, 3);
            }

            SECTION("Reset")
            {
                opt.reset();
                REQUIRE_FALSE(opt.has_value());
                REQUIRE_FALSE(opt);
                CHECK_EQ(opt.get(), nullptr);
            }

            SECTION("Value")
            {
                // Silence [[nodiscard]] warnings
                auto lref = [](heap_optional<int>& o) { return o.value(); };
                CHECK_THROWS_AS(lref(opt), std::bad_optional_access);
                auto clref = [](const heap_optional<int>& o) { return o.value(); };
                CHECK_THROWS_AS(clref(opt), std::bad_optional_access);
                auto rref = [](heap_optional<int>& o) { return std::move(o).value(); };
                CHECK_THROWS_AS(rref(opt), std::bad_optional_access);
            }

            SECTION("Value Or")
            {
                CHECK_EQ(opt.value_or(42), 42);
                CHECK_EQ(const_cast<const heap_optional<int>&>(opt).value_or(42), 42);
                CHECK_EQ(std::move(opt).value_or(42), 42);
            }
        }

        SECTION("With copy and move value")
        {
            auto opt = heap_optional(std::string("hello"));
            using Opt = heap_optional<std::string>;
            static_assert(std::is_same_v<decltype(opt), Opt>);

            REQUIRE(opt.has_value());
            REQUIRE(opt);
            REQUIRE_NE(opt.get(), nullptr);
            CHECK_EQ(*opt, "hello");
            CHECK_EQ(opt->size(), 5);

            SECTION("Emplace data")
            {
                opt.emplace("bonjour");
                REQUIRE(opt.has_value());
                REQUIRE(opt);
                REQUIRE_NE(opt.get(), nullptr);
                CHECK_EQ(*opt, "bonjour");
                CHECK_EQ(opt->size(), 7);
            }

            SECTION("Reset")
            {
                opt.reset();
                REQUIRE_FALSE(opt.has_value());
                REQUIRE_FALSE(opt);
                CHECK_EQ(opt.get(), nullptr);
            }

            SECTION("Value")
            {
                CHECK_EQ(opt.value(), "hello");
                CHECK_EQ(const_cast<const Opt&>(opt).value(), "hello");
                CHECK_EQ(std::move(opt).value(), "hello");
            }

            SECTION("Value Or")
            {
                CHECK_EQ(opt.value_or("world"), "hello");
                CHECK_EQ(const_cast<const Opt&>(opt).value_or("world"), "hello");
                CHECK_EQ(std::move(opt).value_or("world"), "hello");
            }
        }

        SECTION("With move only value")
        {
            auto opt = heap_optional(std::make_unique<int>(3));
            using Opt = heap_optional<std::unique_ptr<int>>;
            static_assert(std::is_same_v<decltype(opt), Opt>);

            REQUIRE(opt.has_value());
            REQUIRE(opt);
            REQUIRE_NE(opt.get(), nullptr);
            CHECK_EQ(**opt, 3);
            CHECK_EQ(*(opt->get()), 3);

            SECTION("Emplace data")
            {
                opt.emplace(std::make_unique<int>(5));
                REQUIRE(opt.has_value());
                REQUIRE(opt);
                REQUIRE_NE(opt.get(), nullptr);
                CHECK_EQ(**opt, 5);
                CHECK_EQ(*(opt->get()), 5);
            }

            SECTION("Reset")
            {
                opt.reset();
                REQUIRE_FALSE(opt.has_value());
                REQUIRE_FALSE(opt);
                CHECK_EQ(opt.get(), nullptr);
            }

            SECTION("Value")
            {
                CHECK_EQ(*(opt.value()), 3);
                CHECK_EQ(*(const_cast<const Opt&>(opt).value()), 3);
                CHECK_EQ(*(std::move(opt).value()), 3);
            }

            SECTION("Value Or")
            {
                CHECK_EQ(*(std::move(opt).value_or(std::make_unique<int>(5))), 3);
            }
        }
    }
}
