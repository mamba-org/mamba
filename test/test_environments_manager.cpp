#include <gtest/gtest.h>

#include "environments_manager.hpp"

namespace mamba
{
    TEST(env_manager, all_envs)
    {
        EnvironmentsManager e;
        auto prefixes = e.list_all_known_prefixes();
        e.register_env(env::expand_user("~/some/env"));
        auto new_prefixes = e.list_all_known_prefixes();
        // the prefix should be cleaned out, because it doesn't have the `conda-meta/history` file
        EXPECT_EQ(new_prefixes.size(), prefixes.size());
    }
}