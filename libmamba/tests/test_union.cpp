#include <gtest/gtest.h>

#include "mamba/core/union_find.hpp"

namespace mamba
{
    UnionFind<int> build_union()
    {
        UnionFind<int> u;
        u.add(0);
        u.add(1);
        u.add(2);
        u.add(3);
        u.add(4);
        u.add(5);
        u.add(6);

        return u;
    }

    TEST(union, build)
    {
        auto u = build_union();
        u.connect(0, 1);
        u.connect(1, 2);
        u.connect(2, 3);
        u.connect(0, 3);
        u.connect(4, 5);
        u.connect(6, 5);
        auto root_zero = u.root(0);
        for (int i = 1; i <= 3; i++)
        {
            EXPECT_TRUE(root_zero == u.root(i));
        }
        auto root_four = u.root(4);
        for (int i = 5; i <= 6; i++)
        {
            EXPECT_TRUE(root_four == u.root(i));
        }
        EXPECT_TRUE(root_zero != root_four);
    }
}
