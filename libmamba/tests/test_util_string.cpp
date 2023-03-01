// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <vector>

#include <gtest/gtest.h>

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/util_string.hpp"

namespace mamba
{
    TEST(util_string, to_lower)
    {
        EXPECT_EQ(to_lower('A'), 'a');
        EXPECT_EQ(to_lower('b'), 'b');
        EXPECT_EQ(to_lower("ThisIsARandomTTTeeesssT"), "thisisarandomttteeessst");
    }

    TEST(util_string, to_upper)
    {
        EXPECT_EQ(to_upper('a'), 'A');
        EXPECT_EQ(to_upper('B'), 'B');
        EXPECT_EQ(to_upper("ThisIsARandomTTTeeesssT"), "THISISARANDOMTTTEEESSST");
    }

    TEST(util_string, starts_with)
    {
        EXPECT_TRUE(starts_with(":hello", ""));
        EXPECT_TRUE(starts_with(":hello", ":"));
        EXPECT_TRUE(starts_with(":hello", ":h"));
        EXPECT_TRUE(starts_with(":hello", ":hello"));
        EXPECT_FALSE(starts_with(":hello", "lo"));
        EXPECT_TRUE(starts_with("áäáœ©gþhëb®hüghœ©®xb", "áäáœ©"));
    }

    TEST(util_string, ends_with)
    {
        EXPECT_TRUE(ends_with("hello&", ""));
        EXPECT_TRUE(ends_with("hello&", "&"));
        EXPECT_TRUE(ends_with("hello&", "o&"));
        EXPECT_TRUE(ends_with("hello&", "hello&"));
        EXPECT_FALSE(ends_with("hello&", "he"));
        EXPECT_TRUE(ends_with("áäáœ©gþhëb®hüghœ©®xb", "©®xb"));
    }

    TEST(util_string, contains)
    {
        EXPECT_TRUE(contains(":hello&", ""));
        EXPECT_TRUE(contains(":hello&", "&"));
        EXPECT_TRUE(contains(":hello&", ":"));
        EXPECT_TRUE(contains(":hello&", "ll"));
        EXPECT_FALSE(contains(":hello&", "eo"));
        EXPECT_TRUE(contains("áäáœ©gþhëb®hüghœ©®xb", "ëb®"));
    }

    TEST(util_string, any_starts_with)
    {
        using StrVec = std::vector<std::string_view>;
        EXPECT_FALSE(any_starts_with(StrVec{}, { "not" }));
        EXPECT_FALSE(any_starts_with(StrVec{}, ""));
        EXPECT_TRUE(any_starts_with(StrVec{ ":hello", "world" }, ""));
        EXPECT_TRUE(any_starts_with(StrVec{ ":hello", "world" }, ":"));
        EXPECT_TRUE(any_starts_with(StrVec{ ":hello", "world" }, ":h"));
        EXPECT_TRUE(any_starts_with(StrVec{ ":hello", "world" }, ":hello"));
        EXPECT_FALSE(any_starts_with(StrVec{ ":hello", "world" }, "orld"));
        EXPECT_TRUE(any_starts_with(StrVec{ "áäáœ©gþhëb", "®hüghœ©®xb" }, "áäá"));
    }

    TEST(util_string, starts_with_any)
    {
        using StrVec = std::vector<std::string_view>;
        EXPECT_TRUE(starts_with_any(":hello", StrVec{ "", "not" }));
        EXPECT_TRUE(starts_with_any(":hello", StrVec{ ":hello", "not" }));
        EXPECT_FALSE(starts_with_any(":hello", StrVec{}));
        EXPECT_FALSE(starts_with_any(":hello", StrVec{ "not", "any" }));
        EXPECT_TRUE(starts_with_any("áäáœ©gþhëb®hüghœ©®xb", StrVec{ "áäáœ©gþhëb", "®hüghœ©®xb" }));
    }

    TEST(util_string, strip)
    {
        EXPECT_EQ(strip("  hello \t\n"), "hello");
        EXPECT_EQ(strip(":::hello%:%", ":%"), "hello");
        EXPECT_EQ(strip(":::hello%:%", ":"), "hello%:%");
        EXPECT_EQ(strip(":::hello%:%", ":"), "hello%:%");
    }

    TEST(util_string, lstrip)
    {
        EXPECT_EQ(lstrip("\n \thello \t\n"), "hello \t\n");
        EXPECT_EQ(lstrip(":::hello%:%", ":%"), "hello%:%");
        EXPECT_EQ(lstrip(":::hello%:%", ":"), "hello%:%");
        EXPECT_EQ(lstrip(":::hello%:%", "%"), ":::hello%:%");
    }

    TEST(util_string, rstrip)
    {
        EXPECT_EQ(rstrip("\n \thello \t\n"), "\n \thello");
        EXPECT_EQ(rstrip(":::hello%:%", "%"), ":::hello%:");
        EXPECT_EQ(rstrip(":::hello%:%", ":%"), ":::hello");
        EXPECT_EQ(rstrip(":::hello%:%", ":"), ":::hello%:%");
    }

    TEST(utils, strip_whitespaces)
    {
        {
            std::string x(strip("   testwhitespacestrip  "));
            EXPECT_EQ(x, "testwhitespacestrip");
            std::string y(rstrip("   testwhitespacestrip  "));
            EXPECT_EQ(y, "   testwhitespacestrip");
            std::string z(lstrip("   testwhitespacestrip  "));
            EXPECT_EQ(z, "testwhitespacestrip  ");
        }
        {
            std::string x(strip("    "));
            EXPECT_EQ(x, "");
            std::string y(rstrip("    "));
            EXPECT_EQ(y, "");
            std::string z(lstrip("    "));
            EXPECT_EQ(z, "");
        }
        {
            std::string x(strip("a"));
            EXPECT_EQ(x, "a");
            std::string y(rstrip("a"));
            EXPECT_EQ(y, "a");
            std::string z(lstrip("a"));
            EXPECT_EQ(z, "a");
        }
        {
            std::string x(strip("  a   "));
            EXPECT_EQ(x, "a");
            std::string y(rstrip(" a  "));
            EXPECT_EQ(y, " a");
            std::string z(lstrip("  a   "));
            EXPECT_EQ(z, "a   ");
        }
        {
            std::string x(strip("abc"));
            EXPECT_EQ(x, "abc");
            std::string y(rstrip("abc"));
            EXPECT_EQ(y, "abc");
            std::string z(lstrip("abc"));
            EXPECT_EQ(z, "abc");
        }
        {
            std::string x(strip(" \r \t  \n   "));
            EXPECT_EQ(x, "");
            std::string y(rstrip("  \r \t  \n  "));
            EXPECT_EQ(y, "");
            std::string z(lstrip("   \r \t  \n "));
            EXPECT_EQ(z, "");
        }
        {
            std::string x(strip("\r \t  \n testwhitespacestrip  \r \t  \n"));
            EXPECT_EQ(x, "testwhitespacestrip");
            std::string y(rstrip("  \r \t  \n testwhitespacestrip  \r \t  \n"));
            EXPECT_EQ(y, "  \r \t  \n testwhitespacestrip");
            std::string z(lstrip("  \r \t  \n testwhitespacestrip \r \t  \n "));
            EXPECT_EQ(z, "testwhitespacestrip \r \t  \n ");
        }
    }

    TEST(util_string, split)
    {
        std::string a = "hello.again.it's.me.mario";
        std::vector<std::string> e1 = { "hello", "again", "it's", "me", "mario" };
        EXPECT_EQ(split(a, "."), e1);

        std::vector<std::string> s2 = { "hello", "again", "it's.me.mario" };
        EXPECT_EQ(split(a, ".", 2), s2);

        EXPECT_EQ(rsplit(a, "."), e1);
        std::vector<std::string> r2 = { "hello.again.it's", "me", "mario" };
        EXPECT_EQ(rsplit(a, ".", 2), r2);

        std::string b = "...";
        auto es1 = std::vector<std::string>{ "", "", "", "" };
        auto es2 = std::vector<std::string>{ "", ".." };
        EXPECT_EQ(split(b, "."), es1);
        EXPECT_EQ(split(b, ".", 1), es2);

        std::vector<std::string> v = { "xtensor==0.12.3" };
        EXPECT_EQ(split(v[0], ":"), v);
        EXPECT_EQ(rsplit(v[0], ":"), v);
        EXPECT_EQ(split(v[0], ":", 2), v);
        EXPECT_EQ(rsplit(v[0], ":", 2), v);

        std::vector<std::string> v2 = { "conda-forge/linux64", "", "xtensor==0.12.3" };
        EXPECT_EQ(split("conda-forge/linux64::xtensor==0.12.3", ":", 2), v2);
        EXPECT_EQ(rsplit("conda-forge/linux64::xtensor==0.12.3", ":", 2), v2);
        std::vector<std::string> v21 = { "conda-forge/linux64:", "xtensor==0.12.3" };

        EXPECT_EQ(rsplit("conda-forge/linux64::xtensor==0.12.3", ":", 1), v21);
    }

    TEST(util_string, join)
    {
        {
            std::vector<std::string> to_join = { "a", "bc", "d" };
            auto joined = join("-", to_join);
            testing::StaticAssertTypeEq<decltype(joined), decltype(to_join)::value_type>();
            EXPECT_EQ(joined, "a-bc-d");
        }
        {
            std::vector<fs::u8path> to_join = { "/a", "bc", "d" };
            auto joined = join("/", to_join);
            testing::StaticAssertTypeEq<decltype(joined), decltype(to_join)::value_type>();
            EXPECT_EQ(joined, "/a/bc/d");
        }
        {
            EXPECT_EQ(join(",", std::vector<std::string>()), "");
        }
    }

    TEST(util_string, join_trunc)
    {
        std::vector<std::string> to_join = { "a", "bc", "d", "e", "f" };
        {
            auto joined = join_trunc(to_join);
            testing::StaticAssertTypeEq<decltype(joined), decltype(to_join)::value_type>();
        }
        EXPECT_EQ(join_trunc(to_join, "-", "..", 5, { 2, 1 }), "a-bc-d-e-f");
        EXPECT_EQ(join_trunc(to_join, ",", "..", 4, { 2, 1 }), "a,bc,..,f");
        EXPECT_EQ(join_trunc(to_join, ",", "..", 4, { 0, 1 }), "..,f");
        EXPECT_EQ(join_trunc(to_join, ",", "..", 4, { 2, 0 }), "a,bc,..");
        EXPECT_EQ(join_trunc(to_join, ",", "..", 4, { 0, 0 }), "..");
        EXPECT_EQ(join_trunc(std::vector<std::string>()), "");
    }

    TEST(util_string, replace_all)
    {
        std::string testbuf = "this is just a test a just a a abc bca";

        replace_all(testbuf, "just", "JU");
        EXPECT_EQ(testbuf, "this is JU a test a JU a a abc bca");
        replace_all(testbuf, "a", "MAMBA");
        EXPECT_EQ(testbuf, "this is JU MAMBA test MAMBA JU MAMBA MAMBA MAMBAbc bcMAMBA");
        replace_all(testbuf, " ", "");
        EXPECT_EQ(testbuf, "thisisJUMAMBAtestMAMBAJUMAMBAMAMBAMAMBAbcbcMAMBA");
        std::string prefix = "/I/am/a/PREFIX\n\nabcdefg\nxyz";

        replace_all(prefix, "/I/am/a/PREFIX", "/Yes/Thats/great/");
        EXPECT_TRUE(starts_with(prefix, "/Yes/Thats/great/\n"));

        std::string testbuf2 = "this is another test wow";
        replace_all(testbuf2, "", "somereplacement");
        EXPECT_EQ(testbuf2, "this is another test wow");

        std::string prefix_unicode = "/I/am/Dörteæœ©æ©fðgb®/PREFIX\n\nabcdefg\nxyz";
        replace_all(prefix_unicode, "/I/am/Dörteæœ©æ©fðgb®/PREFIX", "/home/åéäáßðæœ©ðfßfáðß/123123123");
        EXPECT_EQ(prefix_unicode, "/home/åéäáßðæœ©ðfßfáðß/123123123\n\nabcdefg\nxyz");
    }

    TEST(util_string, concat)
    {
        EXPECT_EQ(concat("aa", std::string("bb"), std::string_view("cc"), 'd'), "aabbccd");
    }

}  // namespace mamba
