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
        std::vector<std::string_view> e1 = {"hello", "again", "it's", "me", "mario"};
        EXPECT_EQ(split(a, '.'), e1);

        std::vector<std::string_view> s2 = {"hello", "again", "it's.me.mario"};
        EXPECT_EQ(split(a, '.', 2), s2);

        EXPECT_EQ(rsplit(a, '.'), e1);
        std::vector<std::string_view> r2 = {"hello.again.it's", "me", "mario"};
        EXPECT_EQ(rsplit(a, '.', 2), r2);

        std::string b = "...";
        EXPECT_EQ(split(b, '.'), std::vector<std::string_view>{});
        EXPECT_EQ(rsplit(b, '.'), std::vector<std::string_view>{});
        EXPECT_EQ(split(b, '.', 1), std::vector<std::string_view>{".."});
        EXPECT_EQ(rsplit(b, '.', 1), std::vector<std::string_view>{".."});

        // std::cout << "RSPLIT 2\n\n";
        // for(auto& el : rsplit(a, '.', 2))
        // {
        //     std::cout << el << std::endl;
        // }
    }
}
