// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <fmt/format.h>

#include "mamba/specs/version.hpp"

using namespace mamba::specs;

TEST_SUITE("version")
{
    TEST_CASE("atom_comparison")
    {
        // No literal
        CHECK_EQ(VersionPartAtom(1), VersionPartAtom(1, ""));
        // lowercase
        CHECK_EQ(VersionPartAtom(1, "dev"), VersionPartAtom(1, "DEV"));
        // All operator comparison for mumerals
        CHECK_NE(VersionPartAtom(1), VersionPartAtom(2, "dev"));
        CHECK_LT(VersionPartAtom(1), VersionPartAtom(2, "dev"));
        CHECK_LE(VersionPartAtom(1), VersionPartAtom(2, "dev"));
        CHECK_GT(VersionPartAtom(2, "dev"), VersionPartAtom(1));
        CHECK_GE(VersionPartAtom(2, "dev"), VersionPartAtom(1));
        // All operator comparison for literals
        CHECK_NE(VersionPartAtom(1, "dev"), VersionPartAtom(1, "a"));
        CHECK_LT(VersionPartAtom(1, "dev"), VersionPartAtom(1, "a"));
        CHECK_LE(VersionPartAtom(1, "dev"), VersionPartAtom(1, "a"));
        CHECK_GT(VersionPartAtom(1, "a"), VersionPartAtom(1, "dev"));
        CHECK_GE(VersionPartAtom(1, "a"), VersionPartAtom(1, "dev"));

        // clang-format off
            auto sorted_atoms = std::array{
               VersionPartAtom{ 1, "*" },
               VersionPartAtom{ 1, "dev" },
               VersionPartAtom{ 1, "_" },
               VersionPartAtom{ 1, "a" },
               VersionPartAtom{ 1, "alpha" },
               VersionPartAtom{ 1, "b" },
               VersionPartAtom{ 1, "beta" },
               VersionPartAtom{ 1, "c" },
               VersionPartAtom{ 1, "r" },
               VersionPartAtom{ 1, "rc" },
               VersionPartAtom{ 1, "" },
               VersionPartAtom{ 1, "post" },
            };
        // clang-format on

        // Strict ordering
        CHECK(std::is_sorted(sorted_atoms.cbegin(), sorted_atoms.cend()));
        // None compare equal (given the is_sorted assumption)
        CHECK_EQ(std::adjacent_find(sorted_atoms.cbegin(), sorted_atoms.cend()), sorted_atoms.cend());
    }

    TEST_CASE("atom_format")
    {
        CHECK_EQ(VersionPartAtom(1, "dev").str(), "1dev");
        CHECK_EQ(VersionPartAtom(2).str(), "2");
    }

    TEST_CASE("version_comparison")
    {
        auto v = Version(0, { { { 1, "post" } } });
        REQUIRE_EQ(v.version().size(), 1);
        REQUIRE_EQ(v.version().front().size(), 1);
        REQUIRE_EQ(v.version().front().front(), VersionPartAtom(1, "post"));

        // Same empty 0!1post version
        CHECK_EQ(Version(0, { { { 1, "post" } } }), Version(0, { { { 1, "post" } } }));
        // Empty trailing atom 0!1a == 0!1a0""
        CHECK_EQ(Version(0, { { { 1, "a" } } }), Version(0, { { { 1, "a" }, {} } }));
        // Empty trailing part 0!1a == 0!1a.0""
        CHECK_EQ(Version(0, { { { 1, "a" } } }), Version(0, { { { 1, "a" } }, { {} } }));
        // Mixed 0!1a0""0"" == 0!1a.0""
        CHECK_EQ(Version(0, { { { 1, "a" }, {}, {} } }), Version(0, { { { 1, "a" } }, { {} } }));

        // Different epoch 0!2post < 1!1dev
        CHECK_LT(Version(0, { { { 2, "post" } } }), Version(1, { { { 1, "dev" } } }));
        CHECK_GE(Version(1, { { { 1, "dev" } } }), Version(0, { { { 2, "post" } } }));
        // Different lenght with dev
        CHECK_LT(Version(0, { { { 1 } }, { { 0, "dev" } } }), Version(0, { { { 1 } } }));
        CHECK_LT(Version(0, { { { 1 } }, { { 0 } }, { { 0, "dev" } } }), Version(0, { { { 1 } } }));
        // Different major 0!1post < 0!2dev
        CHECK_LT(Version(0, { { { 1, "post" } } }), Version(0, { { { 2, "dev" } } }));
        // Different length 0!2"".0"" < 0!11"".0"".0post all operator
        CHECK_NE(Version(0, { { { 2 }, { 0 } } }), Version(0, { { { 11 }, { 0 }, { 0, "post" } } }));
        CHECK_LT(Version(0, { { { 2 }, { 0 } } }), Version(0, { { { 11 }, { 0 }, { 0, "post" } } }));
        CHECK_LE(Version(0, { { { 2 }, { 0 } } }), Version(0, { { { 11 }, { 0 }, { 0, "post" } } }));
        CHECK_GT(Version(0, { { { 11 }, { 0 }, { 0, "post" } } }), Version(0, { { { 2 }, { 0 } } }));
        CHECK_GE(Version(0, { { { 11 }, { 0 }, { 0, "post" } } }), Version(0, { { { 2 }, { 0 } } }));
    }

    TEST_CASE("starts_with")
    {
        SUBCASE("positive")
        {
            // clang-format off
            auto const versions = std::vector<std::tuple<Version, Version>>{
                // 0!1.0.0, 0!1
                {Version(), Version()},
                // 0!1a2post, 0!1a2post
                {Version(0, {{{1, "a"}, {2, "post"}}}), Version(0, {{{1, "a"}, {2, "post"}}})},
                // 0!1a2post, 0!1a2post
                {Version(0, {{{1, "a"}, {2, "post"}}}), Version(0, {{{1, "a"}, {2, "post"}}})},
                // 0!1, 0!1
                {Version(0, {{{1}}}), Version(0, {{{1}}})},
                // 0!1, 0!1.1
                {Version(0, {{{1}}}), Version(0, {{{1}}, {{1}}})},
                // 0!1, 0!1.3
                {Version(0, {{{1}}}), Version(0, {{{1}}, {{3}}})},
                // 0!1, 0!1.1a
                {Version(0, {{{1}}}), Version(0, {{{1}}, {{1, "a"}}})},
                // 0!1, 0!1a
                {Version(0, {{{1}}}), Version(0, {{{1, "a"}}})},
                // 0!1, 0!1.0a
                {Version(0, {{{1}}}), Version(0, {{{1}}, {{0, "a"}}})},
                // 0!1, 0!1post
                {Version(0, {{{1}}}), Version(0, {{{1, "post"}}})},
                // 0!1a, 0!1a.1
                {Version(0, {{{1, "a"}}}), Version(0, {{{1, "a"}}, {{1}}})},
                // 0!1a, 0!1a.1post3
                {Version(0, {{{1, "a"}}}), Version(0, {{{1, "a"}}, {{1, "post"}, {3}}})},
                // 0!1.0.0, 0!1
                {Version(0, {{{1}}, {{0}}, {{0}}}), Version(0, {{{1}}})},
            };
            // clang-format on

            for (const auto& [prefix, ver] : versions)
            {
                // Working around clang compilation issue.
                const auto msg = fmt::format(R"(prefix="{}" version="{}")", prefix.str(), ver.str());
                CAPTURE(msg);
                CHECK(ver.starts_with(prefix));
            }
        }

        SUBCASE("negative")
        {
            // clang-format off
            auto const versions = std::vector<std::tuple<Version, Version>>{
                // 0!1a, 1!1a
                {Version(0, {{{1, "a"}}}), Version(1, {{{1, "a"}}})},
                // 0!2, 0!1
                {Version(0, {{{2}}}), Version(0, {{{1}}})},
                // 0!1, 0!2
                {Version(0, {{{1}}}), Version(0, {{{2}}})},
                // 0!1.2, 0!1.3
                {Version(0, {{{1}}, {{2}}}), Version(0, {{{1}}, {{3}}})},
                // 0!1.2, 0!1.1
                {Version(0, {{{1}}, {{2}}}), Version(0, {{{1}}, {{1}}})},
                // 0!1.2, 0!1
                {Version(0, {{{1}}, {{2}}}), Version(0, {{{1}}})},
                // 0!1a, 0!1b
                {Version(0, {{{1, "a"}}}), Version(0, {{{1, "b"}}})},
                // 0!1.1a, 0!1.1b
                {Version(0, {{{1}}, {{1, "a"}}}), Version(0, {{{1}}, {{1, "b"}}})},
                // 0!1.1a, 0!1.1
                {Version(0, {{{1}}, {{1, "a"}}}), Version(0, {{{1}}, {{1}}})},
            };
            // clang-format on

            for (const auto& [prefix, ver] : versions)
            {
                // Working around clang compilation issue.
                const auto msg = fmt::format(R"(prefix="{}" version="{}")", prefix.str(), ver.str());
                CAPTURE(msg);
                CHECK_FALSE(ver.starts_with(prefix));
            }
        }
    }

    TEST_CASE("compatible_with")
    {
        SUBCASE("positive")
        {
            // clang-format off
            auto const versions = std::vector<std::tuple<std::size_t, Version, Version>>{
                {0, Version(), Version()},
                {1, Version(), Version()},
                // 0!1a2post, 0!1a2post
                {1, Version(0, {{{1, "a"}, {2, "post"}}}), Version(0, {{{1, "a"}, {2, "post"}}})},
                // 0!1, 0!1
                {0, Version(0, {{{1}}}), Version(0, {{{1}}})},
                // 0!1, 0!1
                {0, Version(0, {{{1}}}), Version(0, {{{1}}})},
                // 0!1, 0!2
                {0, Version(0, {{{1}}}), Version(0, {{{2}}})},
                // 0!1, 0!1
                {1, Version(0, {{{1}}}), Version(0, {{{1}}})},
                // 0!1, 0!1.1
                {0, Version(0, {{{1}}}), Version(0, {{{1}}, {{1}}})},
                // 0!1, 0!1.1
                {1, Version(0, {{{1}}}), Version(0, {{{1}}, {{1}}})},
                // 0!1, 0!1.3
                {1, Version(0, {{{1}}}), Version(0, {{{1}}, {{3}}})},
                // 0!1, 0!1.1a
                {0, Version(0, {{{1}}}), Version(0, {{{1}}, {{1, "a"}}})},
                // 0!1a, 0!1
                {0, Version(0, {{{1, "a"}}}), Version(0, {{{1}}})},
                // 0!1a, 0!1b
                {0, Version(0, {{{1, "a"}}}), Version(0, {{{1, "b"}}})},
                // 0!1a, 0!1b
                {1, Version(0, {{{1}}, {{1, "a"}}}), Version(0, {{{1}}, {{1, "b"}}})},
                // 0!1, 0!1post
                {0, Version(0, {{{1}}}), Version(0, {{{1, "post"}}})},
                // 0!1a, 0!1a.1
                {0, Version(0, {{{1, "a"}}}), Version(0, {{{1, "a"}}, {{1}}})},
                // 0!1a, 0!1a.1post3
                {0, Version(0, {{{1, "a"}}}), Version(0, {{{1, "a"}}, {{1, "post"}, {3}}})},
                // 0!1.1a, 0!1.1
                {1, Version(0, {{{1}}, {{1, "a"}}}), Version(0, {{{1}}, {{1}}})},
                // 0!1.0.0, 0!1
                {2, Version(0, {{{1}}, {{0}}, {{0}}}), Version(0, {{{1}}})},
                // 0!1.2.3, 0!1.2.3
                {2, Version(0, {{{1}}, {{2}}, {{3}}}), Version(0, {{{1}}, {{2}}, {{3}}})},
                // 0!1.2.3, 0!1.2.4
                {2, Version(0, {{{1}}, {{2}}, {{3}}}), Version(0, {{{1}}, {{2}}, {{4}}})},
                // 0!1.2, 0!1.3
                {1, Version(0, {{{1}}, {{2}}}), Version(0, {{{1}}, {{3}}})},
            };
            // clang-format on

            for (const auto& [level, older, newer] : versions)
            {
                // Working around clang compilation issue.
                const auto msg = fmt::format(
                    R"(level={} prefix="{}" version="{}")",
                    level,
                    older.str(),
                    newer.str()
                );
                CAPTURE(msg);
                CHECK(newer.compatible_with(older, level));
            }
        }

        SUBCASE("negative")
        {
            // clang-format off
            auto const versions = std::vector<std::tuple<std::size_t, Version, Version>>{
                // 0!1a, 1!1a
                {0, Version(0, {{{1, "a"}}}), Version(1, {{{1, "a"}}})},
                // 0!1, 0!1a
                {0, Version(0, {{{1}}}), Version(0, {{{1, "a"}}})},
                // 0!1, 0!1a.0a
                {0, Version(0, {{{1}}}), Version(0, {{{1}}, {{0, "a"}}})},
                // 0!2, 0!1
                {0, Version(0, {{{2}}}), Version(0, {{{1}}})},
                // 0!1, 0!2
                {1, Version(0, {{{1}}}), Version(0, {{{2}}})},
                // 0!1.2, 0!1.1
                {1, Version(0, {{{1}}, {{2}}}), Version(0, {{{1}}, {{1}}})},
                // 0!1.2, 0!1
                {1, Version(0, {{{1}}, {{2}}}), Version(0, {{{1}}})},
                // 0!1.2.3, 0!1.3.1
                {2, Version(0, {{{1}}, {{2}}, {{3}}}), Version(0, {{{1}}, {{3}}, {{1}}})},
                // 0!1.2.3, 0!1.3a.0
                {2, Version(0, {{{1}}, {{2}}, {{3}}}), Version(0, {{{1}}, {{3, "a"}}, {{0}}})},
                // 0!1.2.3, 0!1.3
                {2, Version(0, {{{1}}, {{2}}, {{3}}}), Version(0, {{{1}}, {{3}}})},
                // 0!1.2.3, 0!2a
                {2, Version(0, {{{1}}, {{2}}, {{3}}}), Version(0, {{{2, "a"}}})},
                // 0!1.2, 0!1.1
                {1, Version(0, {{{1}}, {{2}}}), Version(0, {{{1}}, {{1}}})},
                // 0!1, 0!1.1
                {2, Version(0, {{{1}}}), Version(0, {{{1}}, {{1}}})},
                // 0!1.2, 0!1.1
                {0, Version(0, {{{1}}, {{2}}}), Version(0, {{{1}}, {{1}}})},
            };
            // clang-format on

            for (const auto& [level, older, newer] : versions)
            {
                // Working around clang compilation issue.
                const auto msg = fmt::format(
                    R"(level={} prefix="{}" version="{}")",
                    level,
                    older.str(),
                    newer.str()
                );
                CAPTURE(msg);
                CHECK_FALSE(newer.compatible_with(older, level));
            }
        }
    }

    TEST_CASE("version_format")
    {
        // clang-format off
            CHECK_EQ(
                Version(0, {{{11, "a"}, {0, "post"}}, {{3}}, {{4, "dev"}}}).str(),
                "11a0post.3.4dev"
            );
            CHECK_EQ(
                Version(1, {{{11, "a"}, {0}}, {{3}}, {{4, "dev"}}}).str(),
                "1!11a0.3.4dev"
            );
            CHECK_EQ(
                Version(1, {{{11, "a"}, {0}}, {{3}}, {{4, "dev"}}}, {{{1}}, {{2}}}).str(),
                "1!11a0.3.4dev+1.2"
            );
        // clang-format on
    }

    /**
     * Test from Conda
     *
     * @see https://github.com/conda/conda/blob/main/tests/models/test_version.py
     */
    TEST_CASE("parse")
    {
        // clang-format off
            auto sorted_version = std::vector<std::pair<std::string_view, Version>>{
                {"0.4",         Version(0, {{{0}}, {{4}}})},
                {"0.4.0",       Version(0, {{{0}}, {{4}}, {{0}}})},
                {"0.4.1a.vc11", Version(0, {{{0}}, {{4}}, {{1, "a"}}, {{0, "vc"}, {11}}})},
                {"0.4.1.rc",    Version(0, {{{0}}, {{4}}, {{1}}, {{0, "rc"}}})},
                {"0.4.1.vc11",  Version(0, {{{0}}, {{4}}, {{1}}, {{0, "vc"}, {11}}})},
                {"0.4.1",       Version(0, {{{0}}, {{4}}, {{1}}})},
                {"0.5*",        Version(0, {{{0}}, {{5, "*"}}})},
                {"0.5a1",       Version(0, {{{0}}, {{5, "a"}, {1}}})},
                {"0.5b3",       Version(0, {{{0}}, {{5, "b"}, {3}}})},
                {"0.5C1",       Version(0, {{{0}}, {{5, "c"}, {1}}})},
                {"0.5z",        Version(0, {{{0}}, {{5, "z"}}})},
                {"0.5za",       Version(0, {{{0}}, {{5, "za"}}})},
                {"0.5",         Version(0, {{{0}}, {{5}}})},
                {"0.5_5",       Version(0, {{{0}}, {{5}}, {{5}}})},
                {"0.5-5",       Version(0, {{{0}}, {{5}}, {{5}}})},
                {"0.9.6",       Version(0, {{{0}}, {{9}}, {{6}}})},
                {"0.960923",    Version(0, {{{0}}, {{960923}}})},
                {"1.0",         Version(0, {{{1}}, {{0}}})},
                {"1.0.4a3",     Version(0, {{{1}}, {{0}}, {{4, "a"}, {3}}})},
                {"1.0.4b1",     Version(0, {{{1}}, {{0}}, {{4, "b"}, {1}}})},
                {"1.0.4",       Version(0, {{{1}}, {{0}}, {{4}}})},
                {"1.1dev1",     Version(0, {{{1}}, {{1, "dev"}, {1}}})},
                {"1.1_",        Version(0, {{{1}}, {{1, "_"}}})},
                {"1.1a1",       Version(0, {{{1}}, {{1, "a"}, {1}}})},
                {"1.1.dev1",    Version(0, {{{1}}, {{1}}, {{0, "dev"}, {1}}})},
                {"1.1.a1",      Version(0, {{{1}}, {{1}}, {{0, "a"}, {1}}})},
                {"1.1",         Version(0, {{{1}}, {{1}}})},
                {"1.1.post1",   Version(0, {{{1}}, {{1}}, {{0, "post"}, {1}}})},
                {"1.1.1dev1",   Version(0, {{{1}}, {{1}}, {{1, "dev"}, {1}}})},
                {"1.1.1rc1",    Version(0, {{{1}}, {{1}}, {{1, "rc"}, {1}}})},
                {"1.1.1",       Version(0, {{{1}}, {{1}}, {{1}}})},
                {"1.1.1post1",  Version(0, {{{1}}, {{1}}, {{1, "post"}, {1}}})},
                {"1.1post1",    Version(0, {{{1}}, {{1, "post"}, {1}}})},
                {"2g6",         Version(0, {{{2, "g"}, {6}}})},
                {"2.0b1pr0",    Version(0, {{{2}}, {{0, "b"}, {1, "pr"}, {0}}})},
                {"2.2be.ta29",  Version(0, {{{2}}, {{2, "be"}}, {{0, "ta"}, {29}}})},
                {"2.2be5ta29",  Version(0, {{{2}}, {{2, "be"}, {5, "ta"}, {29}}})},
                {"2.2beta29",   Version(0, {{{2}}, {{2, "beta"}, {29}}})},
                {"2.2.0.1",     Version(0, {{{2}}, {{2}}, {{0}}, {{1}}})},
                {"3.1.1.6",     Version(0, {{{3}}, {{1}}, {{1}}, {{6}}})},
                {"3.2.p.r0",    Version(0, {{{3}}, {{2}}, {{0, "p"}}, {{0, "r"}, {0}}})},
                {"3.2.pr0",     Version(0, {{{3}}, {{2}}, {{0, "pr"}, {0}}})},
                {"3.2.pr.1",    Version(0, {{{3}}, {{2}}, {{0, "pr"}}, {{1}}})},
                {"5.5.kw",      Version(0, {{{5}}, {{5}}, {{0, "kw"}}})},
                {"11g",         Version(0, {{{11, "g"}}})},
                {"14.3.1",      Version(0, {{{14}}, {{3}}, {{1}}})},
                {
                    "14.3.1.post26.g9d75ca2",
                    Version( 0, {{{14}}, {{3}}, {{1}}, {{0, "post"}, {26}}, {{0, "g"}, {9, "d"}, {75, "ca"}, {2}}})
                },
                {"1996.07.12",  Version(0, {{{1996}}, {{7}}, {{12}}})},
                {"1!0.4.1",     Version(1, {{{0}}, {{4}}, {{1}}})},
                {"1!3.1.1.6",   Version(1, {{{3}}, {{1}}, {{1}}, {{6}}})},
                {"2!0.4.1",     Version(2, {{{0}}, {{4}}, {{1}}})},
            };
        // clang-format on
        for (const auto& [raw, expected] : sorted_version)
        {
            CHECK_EQ(Version::parse(raw), expected);
        }

        CHECK(std::is_sorted(
            sorted_version.cbegin(),
            sorted_version.cend(),
            [](const auto& a, const auto& b) { return a.second < b.second; }
        ));

        // Lowercase and strip
        CHECK_EQ(Version::parse("0.4.1.rc"), Version::parse("  0.4.1.RC  "));
        CHECK_EQ(Version::parse("  0.4.1.RC  "), Version::parse("0.4.1.rc"));

        // Functional assertions
        CHECK_EQ(Version::parse("  0.4.rc  "), Version::parse("0.4.RC"));
        CHECK_EQ(Version::parse("0.4"), Version::parse("0.4.0"));
        CHECK_NE(Version::parse("0.4"), Version::parse("0.4.1"));
        CHECK_EQ(Version::parse("0.4.a1"), Version::parse("0.4.0a1"));
        CHECK_NE(Version::parse("0.4.a1"), Version::parse("0.4.1a1"));
    }

    TEST_CASE("parse_invalid")
    {
        // Wrong epoch
        CHECK_THROWS_AS(Version::parse("!1.1"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("-1!1.1"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("foo!1.1"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("0post1!1.1"), std::invalid_argument);

        // Empty parts
        CHECK_THROWS_AS(Version::parse(""), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("  "), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("!2.2"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("0!"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("!"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1."), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1..1"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("5.5..mw"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1.2post+"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1!+1.1"), std::invalid_argument);

        // Repeated delimiters
        CHECK_THROWS_AS(Version::parse("5.5++"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("5.5+1+0.0"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1!2!3.0"), std::invalid_argument);
        }

        CHECK(std::is_sorted(
            sorted_version.cbegin(),
            sorted_version.cend(),
            [](const auto& a, const auto& b) { return a.second < b.second; }
        ));

        // Lowercase and strip
        CHECK_EQ(Version::parse("0.4.1.rc"), Version::parse("  0.4.1.RC  "));
        CHECK_EQ(Version::parse("  0.4.1.RC  "), Version::parse("0.4.1.rc"));

        // Functional assertions
        CHECK_EQ(Version::parse("  0.4.rc  "), Version::parse("0.4.RC"));
        CHECK_EQ(Version::parse("0.4"), Version::parse("0.4.0"));
        CHECK_NE(Version::parse("0.4"), Version::parse("0.4.1"));
        CHECK_EQ(Version::parse("0.4.a1"), Version::parse("0.4.0a1"));
        CHECK_NE(Version::parse("0.4.a1"), Version::parse("0.4.1a1"));
    }

    TEST_CASE("parse_invalid")
    {
        // Wrong epoch
        CHECK_THROWS_AS(Version::parse("!1.1"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("-1!1.1"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("foo!1.1"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("0post1!1.1"), std::invalid_argument);

        // Empty parts
        CHECK_THROWS_AS(Version::parse(""), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("  "), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("!2.2"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("0!"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("!"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1."), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1..1"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("5.5..mw"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1.2post+"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1!+1.1"), std::invalid_argument);

        // Repeated delimiters
        CHECK_THROWS_AS(Version::parse("5.5++"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("5.5+1+0.0"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("1!2!3.0"), std::invalid_argument);

        // '-' and '_' delimiters not allowed together.
        CHECK_THROWS_AS(Version::parse("1-1_1"), std::invalid_argument);

        // Forbidden characters
        CHECK_THROWS_AS(Version::parse("3.5&1"), std::invalid_argument);
        CHECK_THROWS_AS(Version::parse("3.5|1"), std::invalid_argument);
    }

    /**
     * Test from Conda.
     *
     * Some packages (most notably openssl) have incompatible version conventions.
     * In particular, openssl interprets letters as version counters rather than
     * pre-release identifiers. For openssl, the relation
     *
     * 1.0.1 < 1.0.1a  =>  False  # should be true for openssl
     *
     * holds, whereas conda packages use the opposite ordering. You can work-around
     * this problem by appending an underscore to plain version numbers:
     *
     * 1.0.1_ < 1.0.1a =>  True   # ensure correct ordering for openssl
     *
     * @see https://github.com/conda/conda/blob/main/tests/models/test_version.py
     */
    TEST_CASE("parse_openssl")
    {
        // clang-format off
            auto versions = std::vector{
                Version::parse("1.0.1dev"),
                Version::parse("1.0.1_"),  // <- this
                Version::parse("1.0.1a"),
                Version::parse("1.0.1b"),
                Version::parse("1.0.1c"),
                Version::parse("1.0.1d"),
                Version::parse("1.0.1r"),
                Version::parse("1.0.1rc"),
                Version::parse("1.0.1rc1"),
                Version::parse("1.0.1rc2"),
                Version::parse("1.0.1s"),
                Version::parse("1.0.1"),  // <- compared to this
                Version::parse("1.0.1post.a"),
                Version::parse("1.0.1post.b"),
                Version::parse("1.0.1post.z"),
                Version::parse("1.0.1post.za"),
                Version::parse("1.0.2"),
            };
        // clang-format on

        // Strict ordering
        CHECK(std::is_sorted(versions.cbegin(), versions.cend()));
        // None compare equal (given the is_sorted assumption)
        CHECK_EQ(std::adjacent_find(versions.cbegin(), versions.cend()), versions.cend());
    }

    /**
     * Test from Conda slightly modified from the PEP 440 test suite.
     *
     * @see https://github.com/conda/conda/blob/main/tests/models/test_version.py
     * @see https://github.com/pypa/packaging/blob/master/tests/test_version.py
     */
    TEST_CASE("parse_pep440")
    {
        auto versions = std::vector{
            // Implicit epoch of 0
            Version::parse("1.0a1"),
            Version::parse("1.0a2.dev456"),
            Version::parse("1.0a12.dev456"),
            Version::parse("1.0a12"),
            Version::parse("1.0b1.dev456"),
            Version::parse("1.0b2"),
            Version::parse("1.0b2.post345.dev456"),
            Version::parse("1.0b2.post345"),
            Version::parse("1.0c1.dev456"),
            Version::parse("1.0c1"),
            Version::parse("1.0c3"),
            Version::parse("1.0rc2"),
            Version::parse("1.0.dev456"),
            Version::parse("1.0"),
            Version::parse("1.0.post456.dev34"),
            Version::parse("1.0.post456"),
            Version::parse("1.1.dev1"),
            Version::parse("1.2.r32+123456"),
            Version::parse("1.2.rev33+123456"),
            Version::parse("1.2+abc"),
            Version::parse("1.2+abc123def"),
            Version::parse("1.2+abc123"),
            Version::parse("1.2+123abc"),
            Version::parse("1.2+123abc456"),
            Version::parse("1.2+1234.abc"),
            Version::parse("1.2+123456"),
            // Explicit epoch of 1
            Version::parse("1!1.0a1"),
            Version::parse("1!1.0a2.dev456"),
            Version::parse("1!1.0a12.dev456"),
            Version::parse("1!1.0a12"),
            Version::parse("1!1.0b1.dev456"),
            Version::parse("1!1.0b2"),
            Version::parse("1!1.0b2.post345.dev456"),
            Version::parse("1!1.0b2.post345"),
            Version::parse("1!1.0c1.dev456"),
            Version::parse("1!1.0c1"),
            Version::parse("1!1.0c3"),
            Version::parse("1!1.0rc2"),
            Version::parse("1!1.0.dev456"),
            Version::parse("1!1.0"),
            Version::parse("1!1.0.post456.dev34"),
            Version::parse("1!1.0.post456"),
            Version::parse("1!1.1.dev1"),
            Version::parse("1!1.2.r32+123456"),
            Version::parse("1!1.2.rev33+123456"),
            Version::parse("1!1.2+abc"),
            Version::parse("1!1.2+abc123def"),
            Version::parse("1!1.2+abc123"),
            Version::parse("1!1.2+123abc"),
            Version::parse("1!1.2+123abc456"),
            Version::parse("1!1.2+1234.abc"),
            Version::parse("1!1.2+123456"),
        };
        // clang-format on

        // Strict ordering
        CHECK(std::is_sorted(versions.cbegin(), versions.cend()));
        // None compare equal (given the is_sorted assumption)
        CHECK_EQ(std::adjacent_find(versions.cbegin(), versions.cend()), versions.cend());
    }
}
