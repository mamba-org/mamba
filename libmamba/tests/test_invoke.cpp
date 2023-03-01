#include <exception>

#include <gtest/gtest.h>

#include "mamba/core/invoke.hpp"
#include "mamba/core/util_string.hpp"

namespace mamba
{
    TEST(safe_invoke, executes_with_success)
    {
        bool was_called = false;
        auto result = safe_invoke([&] { was_called = true; });
        EXPECT_TRUE(result);
        EXPECT_TRUE(was_called);
    }

    TEST(safe_invoke, catches_std_exceptions)
    {
        const auto message = "expected failure";
        auto result = safe_invoke([&] { throw std::runtime_error(message); });
        EXPECT_FALSE(result);
        EXPECT_TRUE(ends_with(result.error().what(), message)) << result.error().what();
    }

    TEST(safe_invoke, catches_any_exceptions)
    {
        const auto message = "expected failure";
        auto result = safe_invoke([&] { throw message; });
        EXPECT_FALSE(result);
        EXPECT_TRUE(ends_with(result.error().what(), "unknown error")) << result.error().what();
    }

    TEST(safe_invoke, safely_catch_moved_callable_destructor_exception)
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
        EXPECT_FALSE(result);
        EXPECT_TRUE(ends_with(result.error().what(), "unknown error")) << result.error().what();
        EXPECT_TRUE(did_move_happened);
    }

}
