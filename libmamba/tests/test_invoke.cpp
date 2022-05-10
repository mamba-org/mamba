#include <gtest/gtest.h>

#include <exception>

#include "mamba/core/util.hpp"
#include "mamba/core/invoke.hpp"

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
}
