// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>

#include "mamba/fs/filesystem.hpp"
#include "mamba/util/string.hpp"

using namespace mamba::util;

namespace
{
    namespace
    {
        TEST_CASE("to_lower")
        {
            REQUIRE(to_lower('A') == 'a');
            REQUIRE(to_lower('b') == 'b');
            REQUIRE(to_lower("ThisIsARandomTTTeeesssT") == "thisisarandomttteeessst");
        }

        TEST_CASE("to_upper")
        {
            REQUIRE(to_upper('a') == 'A');
            REQUIRE(to_upper('B') == 'B');
            REQUIRE(to_upper("ThisIsARandomTTTeeesssT") == "THISISARANDOMTTTEEESSST");
        }

        TEST_CASE("starts_with")
        {
            REQUIRE(starts_with("", ""));
            REQUIRE_FALSE(starts_with("", ":"));
            REQUIRE_FALSE(starts_with("", ':'));
            REQUIRE(starts_with(":hello", ""));
            REQUIRE(starts_with(":hello", ":"));
            REQUIRE(starts_with(":hello", ':'));
            REQUIRE(starts_with(":hello", ":h"));
            REQUIRE(starts_with(":hello", ":hello"));
            REQUIRE_FALSE(starts_with(":hello", "lo"));
            REQUIRE(starts_with("áäáœ©gþhëb®hüghœ©®xb", "áäáœ©"));
        }

        TEST_CASE("ends_with")
        {
            REQUIRE(ends_with("", ""));
            REQUIRE_FALSE(ends_with("", "&"));
            REQUIRE_FALSE(ends_with("", '&'));
            REQUIRE(ends_with("hello&", ""));
            REQUIRE(ends_with("hello&", "&"));
            REQUIRE(ends_with("hello&", '&'));
            REQUIRE(ends_with("hello&", "o&"));
            REQUIRE(ends_with("hello&", "hello&"));
            REQUIRE_FALSE(ends_with("hello&", "he"));
            REQUIRE(ends_with("áäáœ©gþhëb®hüghœ©®xb", "©®xb"));
        }

        TEST_CASE("string_contains")
        {
            REQUIRE(contains('c', 'c'));
            REQUIRE_FALSE(contains('c', 'a'));
            REQUIRE(contains(":hello&", ""));
            REQUIRE(contains(":hello&", '&'));
            REQUIRE(contains(":hello&", ':'));
            REQUIRE(contains(":hello&", "ll"));
            REQUIRE_FALSE(contains(":hello&", "eo"));
            REQUIRE(contains("áäáœ©gþhëb®hüghœ©®xb", "ëb®"));
            REQUIRE_FALSE(contains("", "ab"));
            REQUIRE(contains("", ""));  // same as Python ``"" in ""``
        }

        TEST_CASE("split_prefix")
        {
            using PrefixTail = decltype(split_prefix("", ""));
            REQUIRE(split_prefix("", "") == PrefixTail{ "", "" });
            REQUIRE(split_prefix("hello", "") == PrefixTail{ "", "hello" });
            REQUIRE(split_prefix("hello", "hello") == PrefixTail{ "hello", "" });
            REQUIRE(split_prefix("", "hello") == PrefixTail{ "", "" });
            REQUIRE(
                split_prefix("https://localhost", "https://") == PrefixTail{ "https://", "localhost" }
            );
            REQUIRE(
                split_prefix("https://localhost", "http://") == PrefixTail{ "", "https://localhost" }
            );
            REQUIRE(split_prefix("aabb", "a") == PrefixTail{ "a", "abb" });
            REQUIRE(split_prefix("", 'a') == PrefixTail{ "", "" });
            REQUIRE(split_prefix("a", 'a') == PrefixTail{ "a", "" });
            REQUIRE(split_prefix("aaa", 'a') == PrefixTail{ "a", "aa" });
            REQUIRE(split_prefix("aabb", 'b') == PrefixTail{ "", "aabb" });
        }

        TEST_CASE("remove_prefix")
        {
            REQUIRE(remove_prefix("", "") == "");
            REQUIRE(remove_prefix("hello", "") == "hello");
            REQUIRE(remove_prefix("hello", "hello") == "");
            REQUIRE(remove_prefix("", "hello") == "");
            REQUIRE(remove_prefix("https://localhost", "https://") == "localhost");
            REQUIRE(remove_prefix("https://localhost", "http://") == "https://localhost");
            REQUIRE(remove_prefix("aabb", "a") == "abb");
            REQUIRE(remove_prefix("", 'a') == "");
            REQUIRE(remove_prefix("a", 'a') == "");
            REQUIRE(remove_prefix("aaa", 'a') == "aa");
            REQUIRE(remove_prefix("aabb", 'b') == "aabb");
        }

        TEST_CASE("split_suffix")
        {
            using HeadSuffix = decltype(split_suffix("", ""));
            REQUIRE(split_suffix("", "") == HeadSuffix{ "", "" });
            REQUIRE(split_suffix("hello", "") == HeadSuffix{ "hello", "" });
            REQUIRE(split_suffix("hello", "hello") == HeadSuffix{ "", "hello" });
            REQUIRE(split_suffix("", "hello") == HeadSuffix{ "", "" });
            REQUIRE(split_suffix("localhost:8080", ":8080") == HeadSuffix{ "localhost", ":8080" });
            REQUIRE(split_suffix("localhost:8080", ":80") == HeadSuffix{ "localhost:8080", "" });
            REQUIRE(split_suffix("aabb", "b") == HeadSuffix{ "aab", "b" });
            REQUIRE(split_suffix("", 'b') == HeadSuffix{ "", "" });
            REQUIRE(split_suffix("b", 'b') == HeadSuffix{ "", "b" });
            REQUIRE(split_suffix("bbb", 'b') == HeadSuffix{ "bb", "b" });
            REQUIRE(split_suffix("aabb", 'a') == HeadSuffix{ "aabb", "" });
        }

        TEST_CASE("remove_suffix")
        {
            REQUIRE(remove_suffix("", "") == "");
            REQUIRE(remove_suffix("hello", "") == "hello");
            REQUIRE(remove_suffix("hello", "hello") == "");
            REQUIRE(remove_suffix("", "hello") == "");
            REQUIRE(remove_suffix("localhost:8080", ":8080") == "localhost");
            REQUIRE(remove_suffix("localhost:8080", ":80") == "localhost:8080");
            REQUIRE(remove_suffix("aabb", "b") == "aab");
            REQUIRE(remove_suffix("", 'b') == "");
            REQUIRE(remove_suffix("b", 'b') == "");
            REQUIRE(remove_suffix("bbb", 'b') == "bb");
            REQUIRE(remove_suffix("aabb", 'a') == "aabb");
        }

        TEST_CASE("any_starts_with")
        {
            using StrVec = std::vector<std::string_view>;
            REQUIRE_FALSE(any_starts_with(StrVec{}, { "not" }));
            REQUIRE_FALSE(any_starts_with(StrVec{}, ""));
            REQUIRE(any_starts_with(StrVec{ ":hello", "world" }, ""));
            REQUIRE(any_starts_with(StrVec{ ":hello", "world" }, ":"));
            REQUIRE(any_starts_with(StrVec{ ":hello", "world" }, ":h"));
            REQUIRE(any_starts_with(StrVec{ ":hello", "world" }, ":hello"));
            REQUIRE_FALSE(any_starts_with(StrVec{ ":hello", "world" }, "orld"));
            REQUIRE(any_starts_with(StrVec{ "áäáœ©gþhëb", "®hüghœ©®xb" }, "áäá"));
        }

        TEST_CASE("starts_with_any")
        {
            using StrVec = std::vector<std::string_view>;
            REQUIRE(starts_with_any(":hello", StrVec{ "", "not" }));
            REQUIRE(starts_with_any(":hello", StrVec{ ":hello", "not" }));
            REQUIRE_FALSE(starts_with_any(":hello", StrVec{}));
            REQUIRE_FALSE(starts_with_any(":hello", StrVec{ "not", "any" }));
            REQUIRE(starts_with_any("áäáœ©gþhëb®hüghœ©®xb", StrVec{ "áäáœ©gþhëb", "®hüghœ©®xb" }));
        }

        TEST_CASE("lstrip")
        {
            REQUIRE(lstrip("\n \thello \t\n") == "hello \t\n");
            REQUIRE(lstrip(":::hello%:%", ":%") == "hello%:%");
            REQUIRE(lstrip(":::hello%:%", ':') == "hello%:%");
            REQUIRE(lstrip(":::hello%:%", '%') == ":::hello%:%");
            REQUIRE(lstrip("", '%') == "");
            REQUIRE(lstrip("aaa", 'a') == "");
            REQUIRE(lstrip("aaa", 'b') == "aaa");
        }

        TEST_CASE("lstrip_parts")
        {
            using StrPair = std::array<std::string_view, 2>;
            REQUIRE(lstrip_parts(":::hello%:%", ":%") == StrPair({ ":::", "hello%:%" }));
            REQUIRE(lstrip_parts(":::hello%:%", ':') == StrPair({ ":::", "hello%:%" }));
            REQUIRE(lstrip_parts(":::hello%:%", '%') == StrPair({ "", ":::hello%:%" }));
            REQUIRE(lstrip_parts("", '%') == StrPair({ "", "" }));
            REQUIRE(lstrip_parts("aaa", 'a') == StrPair({ "aaa", "" }));
            REQUIRE(lstrip_parts("aaa", 'b') == StrPair({ "", "aaa" }));
        }

        TEST_CASE("lstrip_if")
        {
            REQUIRE(lstrip_if("", [](auto) { return true; }) == "");
            REQUIRE(lstrip_if("hello", [](auto) { return true; }) == "");
            REQUIRE(lstrip_if("hello", [](auto) { return false; }) == "hello");
            REQUIRE(
                lstrip_if("\n \thello \t\n", [](auto c) { return !is_alphanum(c); }) == "hello \t\n"
            );
            REQUIRE(lstrip_if("123hello456", [](auto c) { return is_digit(c); }) == "hello456");
        }

        TEST_CASE("lstrip_if_parts")
        {
            using StrPair = std::array<std::string_view, 2>;
            REQUIRE(lstrip_if_parts("", [](auto) { return true; }) == StrPair({ "", "" }));
            REQUIRE(lstrip_if_parts("hello", [](auto) { return true; }) == StrPair({ "hello", "" }));
            REQUIRE(lstrip_if_parts("hello", [](auto) { return false; }) == StrPair({ "", "hello" }));
            REQUIRE(
                lstrip_if_parts("\n \thello \t\n", [](auto c) { return !is_alphanum(c); })
                == StrPair({ "\n \t", "hello \t\n" })
            );
            REQUIRE(
                lstrip_if_parts("123hello456", [](auto c) { return is_digit(c); })
                == StrPair({ "123", "hello456" })
            );
        }

        TEST_CASE("rstrip")
        {
            REQUIRE(rstrip("\n \thello \t\n") == "\n \thello");
            REQUIRE(rstrip(":::hello%:%", '%') == ":::hello%:");
            REQUIRE(rstrip(":::hello%:%", ":%") == ":::hello");
            REQUIRE(rstrip(":::hello%:%", ':') == ":::hello%:%");
            REQUIRE(rstrip("", '%') == "");
            REQUIRE(rstrip("aaa", 'a') == "");
            REQUIRE(rstrip("aaa", 'b') == "aaa");
        }

        TEST_CASE("rstrip_parts")
        {
            using StrPair = std::array<std::string_view, 2>;
            REQUIRE(rstrip_parts(":::hello%:%", '%') == StrPair({ ":::hello%:", "%" }));
            REQUIRE(rstrip_parts(":::hello%:%", ":%") == StrPair({ ":::hello", "%:%" }));
            REQUIRE(rstrip_parts(":::hello%:%", ':') == StrPair({ ":::hello%:%", "" }));
            REQUIRE(rstrip_parts("", '%') == StrPair({ "", "" }));
            REQUIRE(rstrip_parts("aaa", 'a') == StrPair({ "", "aaa" }));
            REQUIRE(rstrip_parts("aaa", 'b') == StrPair({ "aaa", "" }));
        }

        TEST_CASE("rstrip_if")
        {
            REQUIRE(rstrip_if("", [](auto) { return true; }) == "");
            REQUIRE(rstrip_if("hello", [](auto) { return true; }) == "");
            REQUIRE(rstrip_if("hello", [](auto) { return false; }) == "hello");
            REQUIRE(
                rstrip_if("\n \thello \t\n", [](auto c) { return !is_alphanum(c); }) == "\n \thello"
            );
            REQUIRE(rstrip_if("123hello456", [](auto c) { return is_digit(c); }) == "123hello");
        }

        TEST_CASE("rstrip_if_parts")
        {
            using StrPair = std::array<std::string_view, 2>;
            REQUIRE(rstrip_if_parts("", [](auto) { return true; }) == StrPair({ "", "" }));
            REQUIRE(rstrip_if_parts("hello", [](auto) { return true; }) == StrPair({ "", "hello" }));
            REQUIRE(rstrip_if_parts("hello", [](auto) { return false; }) == StrPair({ "hello", "" }));
            REQUIRE(
                rstrip_if_parts("\n \thello \t\n", [](auto c) { return !is_alphanum(c); })
                == StrPair({ "\n \thello", " \t\n" })
            );
            REQUIRE(
                rstrip_if_parts("123hello456", [](auto c) { return is_digit(c); })
                == StrPair({ "123hello", "456" })
            );
        }

        TEST_CASE("strip")
        {
            REQUIRE(strip("  hello \t\n") == "hello");
            REQUIRE(strip(":::hello%:%", ":%") == "hello");
            REQUIRE(strip(":::hello%:%", ':') == "hello%:%");
            REQUIRE(strip("", '%') == "");
            REQUIRE(strip("aaa", 'a') == "");
            REQUIRE(strip("aaa", 'b') == "aaa");
        }

        TEST_CASE("strip_parts")
        {
            using StrTrio = std::array<std::string_view, 3>;
            REQUIRE(strip_parts(":::hello%:%", ":%") == StrTrio({ ":::", "hello", "%:%" }));
            REQUIRE(strip_parts(":::hello%:%", ':') == StrTrio({ ":::", "hello%:%", "" }));
            REQUIRE(strip_parts("", '%') == StrTrio({ "", "", "" }));
            REQUIRE(strip_parts("aaa", 'a') == StrTrio({ "aaa", "", "" }));
            REQUIRE(strip_parts("aaa", 'b') == StrTrio({ "", "aaa", "" }));
        }

        TEST_CASE("strip_if")
        {
            REQUIRE(strip_if("", [](auto) { return true; }) == "");
            REQUIRE(strip_if("hello", [](auto) { return true; }) == "");
            REQUIRE(strip_if("hello", [](auto) { return false; }) == "hello");
            REQUIRE(strip_if("\n \thello \t\n", [](auto c) { return !is_alphanum(c); }) == "hello");
            REQUIRE(strip_if("123hello456", [](auto c) { return is_digit(c); }) == "hello");
        }

        TEST_CASE("strip_if_parts")
        {
            using StrTrio = std::array<std::string_view, 3>;
            REQUIRE(strip_if_parts("", [](auto) { return true; }) == StrTrio({ "", "", "" }));
            REQUIRE(strip_if_parts("hello", [](auto) { return true; }) == StrTrio({ "hello", "", "" }));
            REQUIRE(
                strip_if_parts("hello", [](auto) { return false; }) == StrTrio({ "", "hello", "" })
            );
            REQUIRE(
                strip_if_parts("\n \thello \t\n", [](auto c) { return !is_alphanum(c); })
                == StrTrio({ "\n \t", "hello", " \t\n" })
            );
            REQUIRE(
                strip_if_parts("123hello456", [](auto c) { return is_digit(c); })
                == StrTrio({ "123", "hello", "456" })
            );
        }

        TEST_CASE("strip_whitespaces")
        {
            {
                std::string x(strip("   testwhitespacestrip  "));
                REQUIRE(x == "testwhitespacestrip");
                std::string y(rstrip("   testwhitespacestrip  "));
                REQUIRE(y == "   testwhitespacestrip");
                std::string z(lstrip("   testwhitespacestrip  "));
                REQUIRE(z == "testwhitespacestrip  ");
            }
            {
                std::string x(strip("    "));
                REQUIRE(x == "");
                std::string y(rstrip("    "));
                REQUIRE(y == "");
                std::string z(lstrip("    "));
                REQUIRE(z == "");
            }
            {
                std::string x(strip("a"));
                REQUIRE(x == "a");
                std::string y(rstrip("a"));
                REQUIRE(y == "a");
                std::string z(lstrip("a"));
                REQUIRE(z == "a");
            }
            {
                std::string x(strip("  a   "));
                REQUIRE(x == "a");
                std::string y(rstrip(" a  "));
                REQUIRE(y == " a");
                std::string z(lstrip("  a   "));
                REQUIRE(z == "a   ");
            }
            {
                std::string x(strip("abc"));
                REQUIRE(x == "abc");
                std::string y(rstrip("abc"));
                REQUIRE(y == "abc");
                std::string z(lstrip("abc"));
                REQUIRE(z == "abc");
            }
            {
                std::string x(strip(" \r \t  \n   "));
                REQUIRE(x == "");
                std::string y(rstrip("  \r \t  \n  "));
                REQUIRE(y == "");
                std::string z(lstrip("   \r \t  \n "));
                REQUIRE(z == "");
            }
            {
                std::string x(strip("\r \t  \n testwhitespacestrip  \r \t  \n"));
                REQUIRE(x == "testwhitespacestrip");
                std::string y(rstrip("  \r \t  \n testwhitespacestrip  \r \t  \n"));
                REQUIRE(y == "  \r \t  \n testwhitespacestrip");
                std::string z(lstrip("  \r \t  \n testwhitespacestrip \r \t  \n "));
                REQUIRE(z == "testwhitespacestrip \r \t  \n ");
            }
        }

        TEST_CASE("split_once")
        {
            using Out = std::tuple<std::string_view, std::optional<std::string_view>>;

            REQUIRE(split_once("", '/') == Out{ "", std::nullopt });
            REQUIRE(split_once("/", '/') == Out{ "", "" });
            REQUIRE(split_once("hello/world", '/') == Out{ "hello", "world" });
            REQUIRE(split_once("hello/my/world", '/') == Out{ "hello", "my/world" });
            REQUIRE(split_once("/hello/world", '/') == Out{ "", "hello/world" });

            REQUIRE(split_once("", "/") == Out{ "", std::nullopt });
            REQUIRE(split_once("hello/world", "/") == Out{ "hello", "world" });
            REQUIRE(split_once("hello//world", "//") == Out{ "hello", "world" });
            REQUIRE(split_once("hello/my//world", "/") == Out{ "hello", "my//world" });
            REQUIRE(split_once("hello/my//world", "//") == Out{ "hello/my", "world" });
        }

        TEST_CASE("rsplit_once")
        {
            using Out = std::tuple<std::optional<std::string_view>, std::string_view>;

            REQUIRE(rsplit_once("", '/') == Out{ std::nullopt, "" });
            REQUIRE(rsplit_once("/", '/') == Out{ "", "" });
            REQUIRE(rsplit_once("hello/world", '/') == Out{ "hello", "world" });
            REQUIRE(rsplit_once("hello/my/world", '/') == Out{ "hello/my", "world" });
            REQUIRE(rsplit_once("hello/world/", '/') == Out{ "hello/world", "" });

            REQUIRE(rsplit_once("", "/") == Out{ std::nullopt, "" });
            REQUIRE(rsplit_once("hello/world", "/") == Out{ "hello", "world" });
            REQUIRE(rsplit_once("hello//world", "//") == Out{ "hello", "world" });
            REQUIRE(rsplit_once("hello//my/world", "/") == Out{ "hello//my", "world" });
            REQUIRE(rsplit_once("hello//my/world", "//") == Out{ "hello", "my/world" });
        }

        TEST_CASE("split_once_on_any")
        {
            using Out = std::tuple<std::string_view, std::optional<std::string_view>>;

            REQUIRE(split_once_on_any("", "/") == Out{ "", std::nullopt });
            REQUIRE(split_once_on_any("hello,dear world", ", ") == Out{ "hello", "dear world" });
            REQUIRE(split_once_on_any("hello dear,world", ", ") == Out{ "hello", "dear,world" });
            REQUIRE(split_once_on_any("hello/world", "/") == Out{ "hello", "world" });
            REQUIRE(split_once_on_any("hello//world", "//") == Out{ "hello", "/world" });
            REQUIRE(split_once_on_any("hello/my//world", "/") == Out{ "hello", "my//world" });
            REQUIRE(split_once_on_any("hello/my//world", "//") == Out{ "hello", "my//world" });
        }

        TEST_CASE("rsplit_once_on_any")
        {
            using Out = std::tuple<std::optional<std::string_view>, std::string_view>;

            REQUIRE(rsplit_once_on_any("", "/") == Out{ std::nullopt, "" });
            REQUIRE(rsplit_once_on_any("hello,dear world", ", ") == Out{ "hello,dear", "world" });
            REQUIRE(rsplit_once_on_any("hello dear,world", ", ") == Out{ "hello dear", "world" });
            REQUIRE(rsplit_once_on_any("hello/world", "/") == Out{ "hello", "world" });
            REQUIRE(rsplit_once_on_any("hello//world", "//") == Out{ "hello/", "world" });
            REQUIRE(rsplit_once_on_any("hello/my//world", "/") == Out{ "hello/my/", "world" });
            REQUIRE(rsplit_once_on_any("hello/my//world", "//") == Out{ "hello/my/", "world" });
        }

        TEST_CASE("split")
        {
            std::string a = "hello.again.it's.me.mario";
            std::vector<std::string> e1 = { "hello", "again", "it's", "me", "mario" };
            REQUIRE(split(a, ".") == e1);

            std::vector<std::string> s2 = { "hello", "again", "it's.me.mario" };
            REQUIRE(split(a, ".", 2) == s2);

            REQUIRE(rsplit(a, ".") == e1);
            std::vector<std::string> r2 = { "hello.again.it's", "me", "mario" };
            REQUIRE(rsplit(a, ".", 2) == r2);

            std::string b = "...";
            auto es1 = std::vector<std::string>{ "", "", "", "" };
            auto es2 = std::vector<std::string>{ "", ".." };
            REQUIRE(split(b, ".") == es1);
            REQUIRE(split(b, ".", 1) == es2);

            std::vector<std::string> v = { "xtensor==0.12.3" };
            REQUIRE(split(v[0], ":") == v);
            REQUIRE(rsplit(v[0], ":") == v);
            REQUIRE(split(v[0], ":", 2) == v);
            REQUIRE(rsplit(v[0], ":", 2) == v);

            std::vector<std::string> v2 = { "conda-forge/linux64", "", "xtensor==0.12.3" };
            REQUIRE(split("conda-forge/linux64::xtensor==0.12.3", ":", 2) == v2);
            REQUIRE(rsplit("conda-forge/linux64::xtensor==0.12.3", ":", 2) == v2);
            std::vector<std::string> v21 = { "conda-forge/linux64:", "xtensor==0.12.3" };

            REQUIRE(rsplit("conda-forge/linux64::xtensor==0.12.3", ":", 1) == v21);

            std::vector<std::string> es3 = { "" };
            REQUIRE(split(es3[0], ".") == es3);
            REQUIRE(rsplit(es3[0], ".") == es3);
        }

        TEST_CASE("join")
        {
            {
                std::vector<std::string> to_join = { "a", "bc", "d" };
                auto joined = join("-", to_join);
                static_assert(std::is_same<decltype(joined), decltype(to_join)::value_type>::value);
                REQUIRE(joined == "a-bc-d");
            }
            {
                std::vector<mamba::fs::u8path> to_join = { "/a", "bc", "d" };
                auto joined = join("/", to_join);
                static_assert(std::is_same<decltype(joined), decltype(to_join)::value_type>::value);
                REQUIRE(joined == "/a/bc/d");
            }
            {
                REQUIRE(join(",", std::vector<std::string>()) == "");
            }
        }

        TEST_CASE("join_trunc")
        {
            std::vector<std::string> to_join = { "a", "bc", "d", "e", "f" };
            {
                auto joined = join_trunc(to_join);
                static_assert(std::is_same<decltype(joined), decltype(to_join)::value_type>::value);
            }
            REQUIRE(join_trunc(to_join, "-", "..", 5, { 2, 1 }) == "a-bc-d-e-f");
            REQUIRE(join_trunc(to_join, ",", "..", 4, { 2, 1 }) == "a,bc,..,f");
            REQUIRE(join_trunc(to_join, ",", "..", 4, { 0, 1 }) == "..,f");
            REQUIRE(join_trunc(to_join, ",", "..", 4, { 2, 0 }) == "a,bc,..");
            REQUIRE(join_trunc(to_join, ",", "..", 4, { 0, 0 }) == "..");
            REQUIRE(join_trunc(std::vector<std::string>()) == "");
        }

        TEST_CASE("replace_all")
        {
            std::string testbuf = "this is just a test a just a a abc bca";

            replace_all(testbuf, "just", "JU");
            REQUIRE(testbuf == "this is JU a test a JU a a abc bca");
            replace_all(testbuf, "a", "MAMBA");
            REQUIRE(testbuf == "this is JU MAMBA test MAMBA JU MAMBA MAMBA MAMBAbc bcMAMBA");
            replace_all(testbuf, " ", "");
            REQUIRE(testbuf == "thisisJUMAMBAtestMAMBAJUMAMBAMAMBAMAMBAbcbcMAMBA");
            std::string prefix = "/I/am/a/PREFIX\n\nabcdefg\nxyz";

            replace_all(prefix, "/I/am/a/PREFIX", "/Yes/Thats/great/");
            REQUIRE(starts_with(prefix, "/Yes/Thats/great/\n"));

            std::string testbuf2 = "this is another test wow";
            replace_all(testbuf2, "", "somereplacement");
            REQUIRE(testbuf2 == "this is another test wow");

            std::string prefix_unicode = "/I/am/Dörteæœ©æ©fðgb®/PREFIX\n\nabcdefg\nxyz";
            replace_all(
                prefix_unicode,
                "/I/am/Dörteæœ©æ©fðgb®/PREFIX",
                "/home/åéäáßðæœ©ðfßfáðß/123123123"
            );
            REQUIRE(prefix_unicode == "/home/åéäáßðæœ©ðfßfáðß/123123123\n\nabcdefg\nxyz");
        }

        TEST_CASE("concat")
        {
            REQUIRE(concat("aa", std::string("bb"), std::string_view("cc"), 'd') == "aabbccd");
        }

        TEST_CASE("concat_dedup_splits")
        {
            for (std::string_view sep : { "/", "//", "/////", "./", "./." })
            {
                CAPTURE(sep);

                REQUIRE(concat_dedup_splits("", "", sep) == "");

                REQUIRE(
                    concat_dedup_splits(fmt::format("test{}chan", sep), "", sep)
                    == fmt::format("test{}chan", sep)
                );
                REQUIRE(
                    concat_dedup_splits("", fmt::format("test{}chan", sep), sep)
                    == fmt::format("test{}chan", sep)
                );
                REQUIRE(
                    concat_dedup_splits("test", fmt::format("test{}chan", sep), sep)
                    == fmt::format("test{}chan", sep)
                );
                REQUIRE(concat_dedup_splits("test", "chan", sep) == fmt::format("test{}chan", sep));
                REQUIRE(
                    concat_dedup_splits(fmt::format("test{}chan", sep), "chan", sep)
                    == fmt::format("test{}chan", sep)
                );
                REQUIRE(
                    concat_dedup_splits(fmt::format("test{}chan", sep), fmt::format("chan{}foo", sep), sep)
                    == fmt::format("test{}chan{}foo", sep, sep)
                );
                REQUIRE(
                    concat_dedup_splits(
                        fmt::format("test{}chan-foo", sep),
                        fmt::format("foo{}bar", sep),
                        sep
                    )
                    == fmt::format("test{}chan-foo{}foo{}bar", sep, sep, sep, sep)
                );
                REQUIRE(
                    concat_dedup_splits(
                        fmt::format("ab{}test{}chan", sep, sep),
                        fmt::format("chan{}foo{}ab", sep, sep),
                        sep
                    )
                    == fmt::format("ab{}test{}chan{}foo{}ab", sep, sep, sep, sep)
                );
                REQUIRE(
                    concat_dedup_splits(
                        fmt::format("{}test{}chan", sep, sep),
                        fmt::format("chan{}foo{}", sep, sep),
                        sep
                    )
                    == fmt::format("{}test{}chan{}foo{}", sep, sep, sep, sep)
                );
                REQUIRE(
                    concat_dedup_splits(fmt::format("test{}chan", sep), fmt::format("chan{}test", sep), sep)
                    == fmt::format("test{}chan{}test", sep, sep)
                );
            }

            REQUIRE(concat_dedup_splits("test/chan", "chan/foo", "//") == "test/chan//chan/foo");
            REQUIRE(concat_dedup_splits("test/chan", "chan/foo", '/') == "test/chan/foo");
        }
    }
}  // namespace mamba
