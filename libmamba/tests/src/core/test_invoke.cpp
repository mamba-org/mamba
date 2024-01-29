// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <exception>

#include <doctest/doctest.h>

#include "mamba/core/invoke.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    TEST_SUITE("safe_invoke")
    {
        TEST_CASE("executes_with_success")
        {
            bool was_called = false;
            auto result = safe_invoke([&] { was_called = true; });
            CHECK(result);
            CHECK(was_called);
        }

        TEST_CASE("catches_std_exceptions")
        {
            const auto message = "expected failure";
            auto result = safe_invoke([&] { throw std::runtime_error(message); });
            CHECK_FALSE(result);
            CHECK_MESSAGE(util::ends_with(result.error().what(), message), result.error().what());
        }

        TEST_CASE("catches_any_exceptions")
        {
            const auto message = "expected failure";
            auto result = safe_invoke([&] { throw message; });
            CHECK_FALSE(result);
            CHECK_MESSAGE(
                util::ends_with(result.error().what(), "unknown error"),
                result.error().what()
            );
        }

        TEST_CASE("safely_catch_moved_callable_destructor_exception")
        {
            bool did_move_happened = false;

            struct DoNotDoThisAtHome
            {
                bool& did_move_happened;
                bool owner = true;

                DoNotDoThisAtHome(bool& move_happened)
                    : did_move_happened(move_happened)
                {
                }

                DoNotDoThisAtHome(DoNotDoThisAtHome&& other)
                    : did_move_happened(other.did_move_happened)
                {
                    did_move_happened = true;
                    other.owner = false;
                }

                DoNotDoThisAtHome& operator=(DoNotDoThisAtHome&& other)
                {
                    did_move_happened = other.did_move_happened;
                    did_move_happened = true;
                    other.owner = false;
                    return *this;
                }

                ~DoNotDoThisAtHome() noexcept(false)
                {
                    if (owner)
                    {
                        throw "watever";
                    }
                }

                void operator()() const
                {
                    // do nothing
                }
            };

            auto result = safe_invoke(DoNotDoThisAtHome{ did_move_happened });
            CHECK_FALSE(result);
            CHECK_MESSAGE(
                util::ends_with(result.error().what(), "unknown error"),
                result.error().what()
            );
            CHECK(did_move_happened);
        }
    }

}
