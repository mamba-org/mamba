#include <gtest/gtest.h>

#include "mamba/core/satisfiability_error.hpp"

namespace mamba
{

    TEST(dependency_info, range)
    {
        auto const d = DependencyInfo("foo_bar>=4.3.0,<5.0");
        EXPECT_EQ(d.name(), "foo_bar");
        EXPECT_EQ(d.range(), ">=4.3.0,<5.0");
    }

    TEST(dependency_info, equality)
    {
        auto const d = DependencyInfo("foo-bar==4.3.0");
        EXPECT_EQ(d.name(), "foo-bar");
        EXPECT_EQ(d.range(), "==4.3.0");
    }

    TEST(dependency_info, free)
    {
        auto const d = DependencyInfo("foo7");
        EXPECT_EQ(d.name(), "foo7");
        EXPECT_EQ(d.range(), "");
    }

    TEST(dependency_info, strip)
    {
        auto const d = DependencyInfo(" foo >3.0 ");
        EXPECT_EQ(d.name(), "foo");
        EXPECT_EQ(d.range(), ">3.0");
    }

    TEST(dependency_info, fail)
    {
        EXPECT_ANY_THROW(DependencyInfo("<foo"));
    }
}
