#include <gtest/gtest.h>

#include "util.hpp"

namespace mamba
{
    TEST(util, to_upper_lower)
    {
        std::string a = "ThisIsARandomTTTeeesssT";
        EXPECT_EQ(to_upper(a), "THISISARANDOMTTTEEESSST");
        // std::wstring b = "áäáœ©gþhëb®hüghœ©®xb";
        // EXPECT_EQ(to_upper(b), "ÁÄÁŒ©GÞHËB®HÜGHŒ©®XB");
        // EXPECT_EQ(to_lower(a), "thisisarandomttteeessst");
    }

    TEST(util, split)
    {
        std::string a = "hello.again.it's.me.mario";
        std::vector<std::string> e1 = {"hello", "again", "it's", "me", "mario"};
        EXPECT_EQ(split(a, "."), e1);

        std::vector<std::string> s2 = {"hello", "again", "it's.me.mario"};
        EXPECT_EQ(split(a, ".", 2), s2);

        EXPECT_EQ(rsplit(a, "."), e1);
        std::vector<std::string> r2 = {"hello.again.it's", "me", "mario"};
        EXPECT_EQ(rsplit(a, ".", 2), r2);

        std::string b = "...";
        auto es1 = std::vector<std::string>{"", "", "", ""};
        auto es2 = std::vector<std::string>{"", ".."};
        EXPECT_EQ(split(b, "."), es1);
        EXPECT_EQ(split(b, ".", 1), es2);

        std::vector<std::string> v = {"xtensor==0.12.3"};
        EXPECT_EQ(split(v[0], ":"), v);
        EXPECT_EQ(rsplit(v[0], ":"), v);
        EXPECT_EQ(split(v[0], ":", 2), v);
        EXPECT_EQ(rsplit(v[0], ":", 2), v);

        std::vector<std::string> v2 = {"conda-forge/linux64", "", "xtensor==0.12.3"};
        EXPECT_EQ(split("conda-forge/linux64::xtensor==0.12.3", ":", 2), v2);
        EXPECT_EQ(rsplit("conda-forge/linux64::xtensor==0.12.3", ":", 2), v2);
        std::vector<std::string> v21 = {"conda-forge/linux64:", "xtensor==0.12.3"};

        EXPECT_EQ(rsplit("conda-forge/linux64::xtensor==0.12.3", ":", 1), v21);
    }
}
