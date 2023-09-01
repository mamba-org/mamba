// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include "mamba/core/mamba_fs.hpp"
#include "mamba/util/string.hpp"

namespace mamba::util
{
    TEST_SUITE("util::string")
    {
        TEST_CASE("to_lower")
        {
            CHECK_EQ(to_lower('A'), 'a');
            CHECK_EQ(to_lower('b'), 'b');
            CHECK_EQ(to_lower("ThisIsARandomTTTeeesssT"), "thisisarandomttteeessst");
        }

        TEST_CASE("to_upper")
        {
            CHECK_EQ(to_upper('a'), 'A');
            CHECK_EQ(to_upper('B'), 'B');
            CHECK_EQ(to_upper("ThisIsARandomTTTeeesssT"), "THISISARANDOMTTTEEESSST");
        }

        TEST_CASE("starts_with")
        {
            CHECK(starts_with("", ""));
            CHECK_FALSE(starts_with("", ":"));
            CHECK_FALSE(starts_with("", ':'));
            CHECK(starts_with(":hello", ""));
            CHECK(starts_with(":hello", ":"));
            CHECK(starts_with(":hello", ':'));
            CHECK(starts_with(":hello", ":h"));
            CHECK(starts_with(":hello", ":hello"));
            CHECK_FALSE(starts_with(":hello", "lo"));
            CHECK(starts_with("áäáœ©gþhëb®hüghœ©®xb", "áäáœ©"));
        }

        TEST_CASE("ends_with")
        {
            CHECK(ends_with("", ""));
            CHECK_FALSE(ends_with("", "&"));
            CHECK_FALSE(ends_with("", '&'));
            CHECK(ends_with("hello&", ""));
            CHECK(ends_with("hello&", "&"));
            CHECK(ends_with("hello&", '&'));
            CHECK(ends_with("hello&", "o&"));
            CHECK(ends_with("hello&", "hello&"));
            CHECK_FALSE(ends_with("hello&", "he"));
            CHECK(ends_with("áäáœ©gþhëb®hüghœ©®xb", "©®xb"));
        }

        TEST_CASE("contains")
        {
            CHECK(contains(":hello&", ""));
            CHECK(contains(":hello&", "&"));
            CHECK(contains(":hello&", ":"));
            CHECK(contains(":hello&", "ll"));
            CHECK_FALSE(contains(":hello&", "eo"));
            CHECK(contains("áäáœ©gþhëb®hüghœ©®xb", "ëb®"));
            CHECK_FALSE(contains("", "ab"));
            CHECK(contains("", ""));  // same as Python ``"" in ""``
        }

        TEST_CASE("remove_prefix")
        {
            CHECK_EQ(remove_prefix("", ""), "");
            CHECK_EQ(remove_prefix("hello", ""), "hello");
            CHECK_EQ(remove_prefix("hello", "hello"), "");
            CHECK_EQ(remove_prefix("", "hello"), "");
            CHECK_EQ(remove_prefix("https://localhost", "https://"), "localhost");
            CHECK_EQ(remove_prefix("https://localhost", "http://"), "https://localhost");
            CHECK_EQ(remove_prefix("aabb", "a"), "abb");
            CHECK_EQ(remove_prefix("", 'a'), "");
            CHECK_EQ(remove_prefix("a", 'a'), "");
            CHECK_EQ(remove_prefix("aaa", 'a'), "aa");
            CHECK_EQ(remove_prefix("aabb", 'b'), "aabb");
        }

        TEST_CASE("remove_suffix")
        {
            CHECK_EQ(remove_suffix("", ""), "");
            CHECK_EQ(remove_suffix("hello", ""), "hello");
            CHECK_EQ(remove_suffix("hello", "hello"), "");
            CHECK_EQ(remove_suffix("", "hello"), "");
            CHECK_EQ(remove_suffix("localhost:8080", ":8080"), "localhost");
            CHECK_EQ(remove_suffix("localhost:8080", ":80"), "localhost:8080");
            CHECK_EQ(remove_suffix("aabb", "b"), "aab");
            CHECK_EQ(remove_suffix("", 'b'), "");
            CHECK_EQ(remove_suffix("b", 'b'), "");
            CHECK_EQ(remove_suffix("bbb", 'b'), "bb");
            CHECK_EQ(remove_suffix("aabb", 'a'), "aabb");
        }

        TEST_CASE("any_starts_with")
        {
            using StrVec = std::vector<std::string_view>;
            CHECK_FALSE(any_starts_with(StrVec{}, { "not" }));
            CHECK_FALSE(any_starts_with(StrVec{}, ""));
            CHECK(any_starts_with(StrVec{ ":hello", "world" }, ""));
            CHECK(any_starts_with(StrVec{ ":hello", "world" }, ":"));
            CHECK(any_starts_with(StrVec{ ":hello", "world" }, ":h"));
            CHECK(any_starts_with(StrVec{ ":hello", "world" }, ":hello"));
            CHECK_FALSE(any_starts_with(StrVec{ ":hello", "world" }, "orld"));
            CHECK(any_starts_with(StrVec{ "áäáœ©gþhëb", "®hüghœ©®xb" }, "áäá"));
        }

        TEST_CASE("starts_with_any")
        {
            using StrVec = std::vector<std::string_view>;
            CHECK(starts_with_any(":hello", StrVec{ "", "not" }));
            CHECK(starts_with_any(":hello", StrVec{ ":hello", "not" }));
            CHECK_FALSE(starts_with_any(":hello", StrVec{}));
            CHECK_FALSE(starts_with_any(":hello", StrVec{ "not", "any" }));
            CHECK(starts_with_any("áäáœ©gþhëb®hüghœ©®xb", StrVec{ "áäáœ©gþhëb", "®hüghœ©®xb" }));
        }

        TEST_CASE("lstrip")
        {
            CHECK_EQ(lstrip("\n \thello \t\n"), "hello \t\n");
            CHECK_EQ(lstrip(":::hello%:%", ":%"), "hello%:%");
            CHECK_EQ(lstrip(":::hello%:%", ':'), "hello%:%");
            CHECK_EQ(lstrip(":::hello%:%", '%'), ":::hello%:%");
            CHECK_EQ(lstrip("", '%'), "");
            CHECK_EQ(lstrip("aaa", 'a'), "");
            CHECK_EQ(lstrip("aaa", 'b'), "aaa");
        }

        TEST_CASE("lstrip_parts")
        {
            using StrPair = std::array<std::string_view, 2>;
            CHECK_EQ(lstrip_parts(":::hello%:%", ":%"), StrPair({ ":::", "hello%:%" }));
            CHECK_EQ(lstrip_parts(":::hello%:%", ':'), StrPair({ ":::", "hello%:%" }));
            CHECK_EQ(lstrip_parts(":::hello%:%", '%'), StrPair({ "", ":::hello%:%" }));
            CHECK_EQ(lstrip_parts("", '%'), StrPair({ "", "" }));
            CHECK_EQ(lstrip_parts("aaa", 'a'), StrPair({ "aaa", "" }));
            CHECK_EQ(lstrip_parts("aaa", 'b'), StrPair({ "", "aaa" }));
        }

        TEST_CASE("lstrip_if")
        {
            CHECK_EQ(lstrip_if("", [](auto) { return true; }), "");
            CHECK_EQ(lstrip_if("hello", [](auto) { return true; }), "");
            CHECK_EQ(lstrip_if("hello", [](auto) { return false; }), "hello");
            CHECK_EQ(lstrip_if("\n \thello \t\n", [](auto c) { return !is_alphanum(c); }), "hello \t\n");
            CHECK_EQ(lstrip_if("123hello456", [](auto c) { return is_digit(c); }), "hello456");
        }

        TEST_CASE("lstrip_if_parts")
        {
            using StrPair = std::array<std::string_view, 2>;
            CHECK_EQ(lstrip_if_parts("", [](auto) { return true; }), StrPair({ "", "" }));
            CHECK_EQ(lstrip_if_parts("hello", [](auto) { return true; }), StrPair({ "hello", "" }));
            CHECK_EQ(lstrip_if_parts("hello", [](auto) { return false; }), StrPair({ "", "hello" }));
            CHECK_EQ(
                lstrip_if_parts("\n \thello \t\n", [](auto c) { return !is_alphanum(c); }),
                StrPair({ "\n \t", "hello \t\n" })
            );
            CHECK_EQ(
                lstrip_if_parts("123hello456", [](auto c) { return is_digit(c); }),
                StrPair({ "123", "hello456" })
            );
        }

        TEST_CASE("rstrip")
        {
            CHECK_EQ(rstrip("\n \thello \t\n"), "\n \thello");
            CHECK_EQ(rstrip(":::hello%:%", '%'), ":::hello%:");
            CHECK_EQ(rstrip(":::hello%:%", ":%"), ":::hello");
            CHECK_EQ(rstrip(":::hello%:%", ':'), ":::hello%:%");
            CHECK_EQ(rstrip("", '%'), "");
            CHECK_EQ(rstrip("aaa", 'a'), "");
            CHECK_EQ(rstrip("aaa", 'b'), "aaa");
        }

        TEST_CASE("rstrip_parts")
        {
            using StrPair = std::array<std::string_view, 2>;
            CHECK_EQ(rstrip_parts(":::hello%:%", '%'), StrPair({ ":::hello%:", "%" }));
            CHECK_EQ(rstrip_parts(":::hello%:%", ":%"), StrPair({ ":::hello", "%:%" }));
            CHECK_EQ(rstrip_parts(":::hello%:%", ':'), StrPair({ ":::hello%:%", "" }));
            CHECK_EQ(rstrip_parts("", '%'), StrPair({ "", "" }));
            CHECK_EQ(rstrip_parts("aaa", 'a'), StrPair({ "", "aaa" }));
            CHECK_EQ(rstrip_parts("aaa", 'b'), StrPair({ "aaa", "" }));
        }

        TEST_CASE("rstrip_if")
        {
            CHECK_EQ(rstrip_if("", [](auto) { return true; }), "");
            CHECK_EQ(rstrip_if("hello", [](auto) { return true; }), "");
            CHECK_EQ(rstrip_if("hello", [](auto) { return false; }), "hello");
            CHECK_EQ(rstrip_if("\n \thello \t\n", [](auto c) { return !is_alphanum(c); }), "\n \thello");
            CHECK_EQ(rstrip_if("123hello456", [](auto c) { return is_digit(c); }), "123hello");
        }

        TEST_CASE("rstrip_if_parts")
        {
            using StrPair = std::array<std::string_view, 2>;
            CHECK_EQ(rstrip_if_parts("", [](auto) { return true; }), StrPair({ "", "" }));
            CHECK_EQ(rstrip_if_parts("hello", [](auto) { return true; }), StrPair({ "", "hello" }));
            CHECK_EQ(rstrip_if_parts("hello", [](auto) { return false; }), StrPair({ "hello", "" }));
            CHECK_EQ(
                rstrip_if_parts("\n \thello \t\n", [](auto c) { return !is_alphanum(c); }),
                StrPair({ "\n \thello", " \t\n" })
            );
            CHECK_EQ(
                rstrip_if_parts("123hello456", [](auto c) { return is_digit(c); }),
                StrPair({ "123hello", "456" })
            );
        }

        TEST_CASE("strip")
        {
            CHECK_EQ(strip("  hello \t\n"), "hello");
            CHECK_EQ(strip(":::hello%:%", ":%"), "hello");
            CHECK_EQ(strip(":::hello%:%", ':'), "hello%:%");
            CHECK_EQ(strip("", '%'), "");
            CHECK_EQ(strip("aaa", 'a'), "");
            CHECK_EQ(strip("aaa", 'b'), "aaa");
        }

        TEST_CASE("strip_parts")
        {
            using StrTrio = std::array<std::string_view, 3>;
            CHECK_EQ(strip_parts(":::hello%:%", ":%"), StrTrio({ ":::", "hello", "%:%" }));
            CHECK_EQ(strip_parts(":::hello%:%", ':'), StrTrio({ ":::", "hello%:%", "" }));
            CHECK_EQ(strip_parts("", '%'), StrTrio({ "", "", "" }));
            CHECK_EQ(strip_parts("aaa", 'a'), StrTrio({ "aaa", "", "" }));
            CHECK_EQ(strip_parts("aaa", 'b'), StrTrio({ "", "aaa", "" }));
        }

        TEST_CASE("strip_if")
        {
            CHECK_EQ(strip_if("", [](auto) { return true; }), "");
            CHECK_EQ(strip_if("hello", [](auto) { return true; }), "");
            CHECK_EQ(strip_if("hello", [](auto) { return false; }), "hello");
            CHECK_EQ(strip_if("\n \thello \t\n", [](auto c) { return !is_alphanum(c); }), "hello");
            CHECK_EQ(strip_if("123hello456", [](auto c) { return is_digit(c); }), "hello");
        }

        TEST_CASE("strip_if_parts")
        {
            using StrTrio = std::array<std::string_view, 3>;
            CHECK_EQ(strip_if_parts("", [](auto) { return true; }), StrTrio({ "", "", "" }));
            CHECK_EQ(strip_if_parts("hello", [](auto) { return true; }), StrTrio({ "hello", "", "" }));
            CHECK_EQ(strip_if_parts("hello", [](auto) { return false; }), StrTrio({ "", "hello", "" }));
            CHECK_EQ(
                strip_if_parts("\n \thello \t\n", [](auto c) { return !is_alphanum(c); }),
                StrTrio({ "\n \t", "hello", " \t\n" })
            );
            CHECK_EQ(
                strip_if_parts("123hello456", [](auto c) { return is_digit(c); }),
                StrTrio({ "123", "hello", "456" })
            );
        }

        TEST_CASE("strip_whitespaces")
        {
            {
                std::string x(strip("   testwhitespacestrip  "));
                CHECK_EQ(x, "testwhitespacestrip");
                std::string y(rstrip("   testwhitespacestrip  "));
                CHECK_EQ(y, "   testwhitespacestrip");
                std::string z(lstrip("   testwhitespacestrip  "));
                CHECK_EQ(z, "testwhitespacestrip  ");
            }
            {
                std::string x(strip("    "));
                CHECK_EQ(x, "");
                std::string y(rstrip("    "));
                CHECK_EQ(y, "");
                std::string z(lstrip("    "));
                CHECK_EQ(z, "");
            }
            {
                std::string x(strip("a"));
                CHECK_EQ(x, "a");
                std::string y(rstrip("a"));
                CHECK_EQ(y, "a");
                std::string z(lstrip("a"));
                CHECK_EQ(z, "a");
            }
            {
                std::string x(strip("  a   "));
                CHECK_EQ(x, "a");
                std::string y(rstrip(" a  "));
                CHECK_EQ(y, " a");
                std::string z(lstrip("  a   "));
                CHECK_EQ(z, "a   ");
            }
            {
                std::string x(strip("abc"));
                CHECK_EQ(x, "abc");
                std::string y(rstrip("abc"));
                CHECK_EQ(y, "abc");
                std::string z(lstrip("abc"));
                CHECK_EQ(z, "abc");
            }
            {
                std::string x(strip(" \r \t  \n   "));
                CHECK_EQ(x, "");
                std::string y(rstrip("  \r \t  \n  "));
                CHECK_EQ(y, "");
                std::string z(lstrip("   \r \t  \n "));
                CHECK_EQ(z, "");
            }
            {
                std::string x(strip("\r \t  \n testwhitespacestrip  \r \t  \n"));
                CHECK_EQ(x, "testwhitespacestrip");
                std::string y(rstrip("  \r \t  \n testwhitespacestrip  \r \t  \n"));
                CHECK_EQ(y, "  \r \t  \n testwhitespacestrip");
                std::string z(lstrip("  \r \t  \n testwhitespacestrip \r \t  \n "));
                CHECK_EQ(z, "testwhitespacestrip \r \t  \n ");
            }
        }

        TEST_CASE("split")
        {
            std::string a = "hello.again.it's.me.mario";
            std::vector<std::string> e1 = { "hello", "again", "it's", "me", "mario" };
            CHECK_EQ(split(a, "."), e1);

            std::vector<std::string> s2 = { "hello", "again", "it's.me.mario" };
            CHECK_EQ(split(a, ".", 2), s2);

            CHECK_EQ(rsplit(a, "."), e1);
            std::vector<std::string> r2 = { "hello.again.it's", "me", "mario" };
            CHECK_EQ(rsplit(a, ".", 2), r2);

            std::string b = "...";
            auto es1 = std::vector<std::string>{ "", "", "", "" };
            auto es2 = std::vector<std::string>{ "", ".." };
            CHECK_EQ(split(b, "."), es1);
            CHECK_EQ(split(b, ".", 1), es2);

            std::vector<std::string> v = { "xtensor==0.12.3" };
            CHECK_EQ(split(v[0], ":"), v);
            CHECK_EQ(rsplit(v[0], ":"), v);
            CHECK_EQ(split(v[0], ":", 2), v);
            CHECK_EQ(rsplit(v[0], ":", 2), v);

            std::vector<std::string> v2 = { "conda-forge/linux64", "", "xtensor==0.12.3" };
            CHECK_EQ(split("conda-forge/linux64::xtensor==0.12.3", ":", 2), v2);
            CHECK_EQ(rsplit("conda-forge/linux64::xtensor==0.12.3", ":", 2), v2);
            std::vector<std::string> v21 = { "conda-forge/linux64:", "xtensor==0.12.3" };

            CHECK_EQ(rsplit("conda-forge/linux64::xtensor==0.12.3", ":", 1), v21);
        }

        TEST_CASE("join")
        {
            {
                std::vector<std::string> to_join = { "a", "bc", "d" };
                auto joined = join("-", to_join);
                static_assert(std::is_same<decltype(joined), decltype(to_join)::value_type>::value);
                CHECK_EQ(joined, "a-bc-d");
            }
            {
                std::vector<fs::u8path> to_join = { "/a", "bc", "d" };
                auto joined = join("/", to_join);
                static_assert(std::is_same<decltype(joined), decltype(to_join)::value_type>::value);
                CHECK_EQ(joined, "/a/bc/d");
            }
            {
                CHECK_EQ(join(",", std::vector<std::string>()), "");
            }
        }

        TEST_CASE("join_trunc")
        {
            std::vector<std::string> to_join = { "a", "bc", "d", "e", "f" };
            {
                auto joined = join_trunc(to_join);
                static_assert(std::is_same<decltype(joined), decltype(to_join)::value_type>::value);
            }
            CHECK_EQ(join_trunc(to_join, "-", "..", 5, { 2, 1 }), "a-bc-d-e-f");
            CHECK_EQ(join_trunc(to_join, ",", "..", 4, { 2, 1 }), "a,bc,..,f");
            CHECK_EQ(join_trunc(to_join, ",", "..", 4, { 0, 1 }), "..,f");
            CHECK_EQ(join_trunc(to_join, ",", "..", 4, { 2, 0 }), "a,bc,..");
            CHECK_EQ(join_trunc(to_join, ",", "..", 4, { 0, 0 }), "..");
            CHECK_EQ(join_trunc(std::vector<std::string>()), "");
        }

        TEST_CASE("replace_all")
        {
            std::string testbuf = "this is just a test a just a a abc bca";

            replace_all(testbuf, "just", "JU");
            CHECK_EQ(testbuf, "this is JU a test a JU a a abc bca");
            replace_all(testbuf, "a", "MAMBA");
            CHECK_EQ(testbuf, "this is JU MAMBA test MAMBA JU MAMBA MAMBA MAMBAbc bcMAMBA");
            replace_all(testbuf, " ", "");
            CHECK_EQ(testbuf, "thisisJUMAMBAtestMAMBAJUMAMBAMAMBAMAMBAbcbcMAMBA");
            std::string prefix = "/I/am/a/PREFIX\n\nabcdefg\nxyz";

            replace_all(prefix, "/I/am/a/PREFIX", "/Yes/Thats/great/");
            CHECK(starts_with(prefix, "/Yes/Thats/great/\n"));

            std::string testbuf2 = "this is another test wow";
            replace_all(testbuf2, "", "somereplacement");
            CHECK_EQ(testbuf2, "this is another test wow");

            std::string prefix_unicode = "/I/am/Dörteæœ©æ©fðgb®/PREFIX\n\nabcdefg\nxyz";
            replace_all(
                prefix_unicode,
                "/I/am/Dörteæœ©æ©fðgb®/PREFIX",
                "/home/åéäáßðæœ©ðfßfáðß/123123123"
            );
            CHECK_EQ(prefix_unicode, "/home/åéäáßðæœ©ðfßfáðß/123123123\n\nabcdefg\nxyz");
        }

        TEST_CASE("concat")
        {
            CHECK_EQ(concat("aa", std::string("bb"), std::string_view("cc"), 'd'), "aabbccd");
        }

        TEST_CASE("get_common_parts")
        {
            CHECK_EQ(get_common_parts("", "", "/"), "");
            CHECK_EQ(get_common_parts("", "test", "/"), "");
            CHECK_EQ(get_common_parts("test", "test", "/"), "test");
            CHECK_EQ(get_common_parts("test/chan", "test/chan", "/"), "test/chan");
            CHECK_EQ(get_common_parts("st/ch", "test/chan", "/"), "");
            CHECK_EQ(get_common_parts("st/chan", "test/chan", "/"), "chan");
            CHECK_EQ(get_common_parts("st/chan/abc", "test/chan/abc", "/"), "chan/abc");
            CHECK_EQ(get_common_parts("test/ch", "test/chan", "/"), "test");
            CHECK_EQ(get_common_parts("test/an/abc", "test/chan/abc", "/"), "abc");
            CHECK_EQ(get_common_parts("test/chan/label", "label/abcd/xyz", "/"), "label");
            CHECK_EQ(get_common_parts("test/chan/label", "chan/label/abcd", "/"), "chan/label");
            CHECK_EQ(get_common_parts("test/chan/label", "abcd/chan/label", "/"), "chan/label");
            CHECK_EQ(get_common_parts("test", "abcd", "/"), "");
            CHECK_EQ(get_common_parts("test", "abcd/xyz", "/"), "");
            CHECK_EQ(get_common_parts("test/xyz", "abcd/xyz", "/"), "xyz");
            CHECK_EQ(get_common_parts("test/xyz", "abcd/gef", "/"), "");
            CHECK_EQ(get_common_parts("abcd/test", "abcd/xyz", "/"), "");

            CHECK_EQ(get_common_parts("", "", "."), "");
            CHECK_EQ(get_common_parts("", "test", "."), "");
            CHECK_EQ(get_common_parts("test", "test", "."), "test");
            CHECK_EQ(get_common_parts("test.chan", "test.chan", "."), "test.chan");
            CHECK_EQ(get_common_parts("test.chan.label", "chan.label.abcd", "."), "chan.label");
            CHECK_EQ(get_common_parts("test/chan/label", "chan/label/abcd", "."), "");
            CHECK_EQ(get_common_parts("st/ch", "test/chan", "."), "");
            CHECK_EQ(get_common_parts("st.ch", "test.chan", "."), "");

            CHECK_EQ(get_common_parts("test..chan", "test..chan", ".."), "test..chan");
        }
    }

}  // namespace mamba
