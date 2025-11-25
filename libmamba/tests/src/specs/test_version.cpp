// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <vector>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>

#include "mamba/specs/version.hpp"
#include "mamba/util/string.hpp"

using namespace mamba::specs;

namespace
{
    TEST_CASE("VersionPartAtom", "[mamba::specs][mamba::specs::Version]")
    {
        // No literal
        REQUIRE(VersionPartAtom(1) == VersionPartAtom(1, ""));
        // lowercase
        REQUIRE(VersionPartAtom(1, "dev") == VersionPartAtom(1, "DEV"));
        // All operator comparison for mumerals
        REQUIRE(VersionPartAtom(1) != VersionPartAtom(2, "dev"));
        REQUIRE(VersionPartAtom(1) < VersionPartAtom(2, "dev"));
        REQUIRE(VersionPartAtom(1) <= VersionPartAtom(2, "dev"));
        REQUIRE(VersionPartAtom(2, "dev") > VersionPartAtom(1));
        REQUIRE(VersionPartAtom(2, "dev") >= VersionPartAtom(1));
        // All operator comparison for literals
        REQUIRE(VersionPartAtom(1, "dev") != VersionPartAtom(1, "a"));
        REQUIRE(VersionPartAtom(1, "dev") < VersionPartAtom(1, "a"));
        REQUIRE(VersionPartAtom(1, "dev") <= VersionPartAtom(1, "a"));
        REQUIRE(VersionPartAtom(1, "a") > VersionPartAtom(1, "dev"));
        REQUIRE(VersionPartAtom(1, "a") >= VersionPartAtom(1, "dev"));

        // clang-format off
        auto const sorted_atoms = std::array{
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
        REQUIRE(std::is_sorted(sorted_atoms.cbegin(), sorted_atoms.cend()));
        // None compare equal (given the is_sorted assumption)
        REQUIRE(std::adjacent_find(sorted_atoms.cbegin(), sorted_atoms.cend()) == sorted_atoms.cend());
    }

    TEST_CASE("VersionPartAtom::str", "[mamba::specs][mamba::specs::Version]")
    {
        REQUIRE(VersionPartAtom(1, "dev").to_string() == "1dev");
        REQUIRE(VersionPartAtom(2).to_string() == "2");
    }

    TEST_CASE("VersionPart", "[mamba::specs][mamba::specs::Version]")
    {
        // clang-format off
        REQUIRE(VersionPart{{1, "dev"}} == VersionPart{{1, "dev"}});
        REQUIRE(VersionPart{{1, "dev"}} == VersionPart{{1, "dev"}, {0, ""}});
        REQUIRE(VersionPart{{1, "dev"}, {2, ""}} == VersionPart{{1, "dev"}, {2, ""}});
        REQUIRE(VersionPart({{0, "dev"}, {2, ""}}, true) == VersionPart{{0, "dev"}, {2, ""}});
        REQUIRE(VersionPart{{0, "dev"} } != VersionPart{{0, "dev"}, {2, ""}});

        auto const sorted_parts = std::array{
            VersionPart{{0, ""}},
            VersionPart{{1, "dev"}, {0, "alpha"}},
            VersionPart{{1, "dev"}},
            VersionPart{{1, "dev"}, {1, "dev"}},
            VersionPart{{2, "dev"}, {1, "dev"}},
            VersionPart{{2, ""}},
            VersionPart{{2, ""}, {0, "post"}},
        };
        // clang-format on

        // Strict ordering
        REQUIRE(std::is_sorted(sorted_parts.cbegin(), sorted_parts.cend()));
        // None compare equal (given the is_sorted assumption)
        REQUIRE(std::adjacent_find(sorted_parts.cbegin(), sorted_parts.cend()) == sorted_parts.cend());
    }

    TEST_CASE("VersionPart::str", "[mamba::specs][mamba::specs::Version]")
    {
        REQUIRE(VersionPart{ { 1, "dev" } }.to_string() == "1dev");
        REQUIRE(VersionPart{ { 1, "dev" }, { 2, "" } }.to_string() == "1dev2");
        REQUIRE(
            VersionPart{ { 1, "dev" }, { 2, "foo" }, { 33, "bar" } }.to_string() == "1dev2foo33bar"
        );
        REQUIRE(VersionPart({ { 0, "dev" }, { 2, "" } }, false).to_string() == "0dev2");
        REQUIRE(VersionPart({ { 0, "dev" }, { 2, "" } }, true).to_string() == "dev2");
        REQUIRE(VersionPart({ { 0, "dev" } }, true).to_string() == "dev");
        REQUIRE(VersionPart({ { 0, "" } }, true).to_string() == "0");
    }

    TEST_CASE("Version cmp", "[mamba::specs][mamba::specs::Version]")
    {
        auto v = Version(0, { { { 1, "post" } } });
        REQUIRE(v.version().size() == 1);
        REQUIRE(v.version().front().atoms.size() == 1);
        REQUIRE(v.version().front().atoms.front() == VersionPartAtom(1, "post"));

        // Same empty 0!1post version
        REQUIRE(Version(0, { { { 1, "post" } } }) == Version(0, { { { 1, "post" } } }));
        // Empty trailing atom 0!1a == 0!1a0""
        REQUIRE(Version(0, { { { 1, "a" } } }) == Version(0, { { { 1, "a" }, {} } }));
        // Empty trailing part 0!1a == 0!1a.0""
        REQUIRE(Version(0, { { { 1, "a" } } }) == Version(0, { { { 1, "a" } }, { {} } }));
        // Mixed 0!1a0""0"" == 0!1a.0""
        REQUIRE(Version(0, { { { 1, "a" }, {}, {} } }) == Version(0, { { { 1, "a" } }, { {} } }));

        // Different epoch 0!2post < 1!1dev
        REQUIRE(Version(0, { { { 2, "post" } } }) < Version(1, { { { 1, "dev" } } }));
        REQUIRE(Version(1, { { { 1, "dev" } } }) >= Version(0, { { { 2, "post" } } }));
        // Different length with dev
        REQUIRE(Version(0, { { { 1 } }, { { 0, "dev" } } }) < Version(0, { { { 1 } } }));
        REQUIRE(Version(0, { { { 1 } }, { { 0 } }, { { 0, "dev" } } }) < Version(0, { { { 1 } } }));
        // Different major 0!1post < 0!2dev
        REQUIRE(Version(0, { { { 1, "post" } } }) < Version(0, { { { 2, "dev" } } }));
        // Different length 0!2"".0"" < 0!11"".0"".0post all operator
        REQUIRE(Version(0, { { { 2 }, { 0 } } }) != Version(0, { { { 11 }, { 0 }, { 0, "post" } } }));
        REQUIRE(Version(0, { { { 2 }, { 0 } } }) < Version(0, { { { 11 }, { 0 }, { 0, "post" } } }));
        REQUIRE(Version(0, { { { 2 }, { 0 } } }) <= Version(0, { { { 11 }, { 0 }, { 0, "post" } } }));
        REQUIRE(Version(0, { { { 11 }, { 0 }, { 0, "post" } } }) > Version(0, { { { 2 }, { 0 } } }));
        REQUIRE(Version(0, { { { 11 }, { 0 }, { 0, "post" } } }) >= Version(0, { { { 2 }, { 0 } } }));
    }

    TEST_CASE("Version starts_with", "[mamba::specs][mamba::specs::Version]")
    {
        SECTION("positive")
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
                // 0!0 0!0.4.1
                {Version(0, {{{0}}}), Version(0, {{{0}}, {{4}}, {{1}}})},
                // 0!0.4 0!0.4.1
                {Version(0, {{{0}}, {{4}}}), Version(0, {{{0}}, {{4}}, {{1}}})},
                // 0!0.4 0!0.4.1p1
                {Version(0, {{{0}}, {{4}}}), Version(0, {{{0}}, {{4}}, {{1, "p"}, {1}}})},
                // 0!0.4.1p 0!0.4.1p1
                {Version(0, {{{0}}, {{4}}, {{1, "p"}}}), Version(0, {{{0}}, {{4}}, {{1, "p"}, {1}}})},
                // 0!0.4.1 0!0.4.1+1.3
                {Version(0, {{{0}}, {{4}}, {{1}}}), Version(0, {{{0}}, {{4}}, {{1}}}, {{{1}}, {{3}}})},
                // 0!0.4.1+1 0!0.4.1+1.3
                {Version(0, {{{0}}, {{4}}, {{1}}}, {{{1}}}), Version(0, {{{0}}, {{4}}, {{1}}}, {{{1}}, {{3}}})},
            };
            // clang-format on

            for (const auto& [prefix, ver] : versions)
            {
                // Working around clang compilation issue.
                const auto msg = fmt::format(
                    R"(prefix="{}" version="{}")",
                    prefix.to_string(),
                    ver.to_string()
                );
                CAPTURE(msg);
                REQUIRE(ver.starts_with(prefix));
            }
        }

        SECTION("negative")
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
                const auto msg = fmt::format(
                    R"(prefix="{}" version="{}")",
                    prefix.to_string(),
                    ver.to_string()
                );
                CAPTURE(msg);
                REQUIRE_FALSE(ver.starts_with(prefix));
            }
        }
    }

    TEST_CASE("Version compatible_with", "[mamba::specs][mamba::specs::Version]")
    {
        SECTION("positive")
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
                    older.to_string(),
                    newer.to_string()
                );
                CAPTURE(msg);
                REQUIRE(newer.compatible_with(older, level));
            }
        }

        SECTION("negative")
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
                    older.to_string(),
                    newer.to_string()
                );
                CAPTURE(msg);
                REQUIRE_FALSE(newer.compatible_with(older, level));
            }
        }
    }

    TEST_CASE("Version::str", "[mamba::specs][mamba::specs::Version]")
    {
        SECTION("11a0post.3.4dev")
        {
            const auto v = Version(0, { { { 11, "a" }, { 0, "post" } }, { { 3 } }, { { 4, "dev" } } });
            REQUIRE(v.to_string() == "11a0post.3.4dev");
            REQUIRE(v.to_string(1) == "11a0post");
            REQUIRE(v.to_string(2) == "11a0post.3");
            REQUIRE(v.to_string(3) == "11a0post.3.4dev");
            REQUIRE(v.to_string(4) == "11a0post.3.4dev.0");
            REQUIRE(v.to_string(5) == "11a0post.3.4dev.0.0");
        }

        SECTION("1!11a0.3.4dev")
        {
            const auto v = Version(1, { { { 11, "a" }, { 0 } }, { { 3 } }, { { 4, "dev" } } });
            REQUIRE(v.to_string() == "1!11a0.3.4dev");
            REQUIRE(v.to_string(1) == "1!11a0");
            REQUIRE(v.to_string(2) == "1!11a0.3");
            REQUIRE(v.to_string(3) == "1!11a0.3.4dev");
            REQUIRE(v.to_string(4) == "1!11a0.3.4dev.0");
        }

        SECTION("1!11a0.3.4dev+1.2")
        {
            const auto v = Version(
                1,
                { { { 11, "a" }, { 0 } }, { { 3 } }, { { 4, "dev" } } },
                { { { 1 } }, { { 2 } } }
            );
            REQUIRE(v.to_string() == "1!11a0.3.4dev+1.2");
            REQUIRE(v.to_string(1) == "1!11a0+1");
            REQUIRE(v.to_string(2) == "1!11a0.3+1.2");
            REQUIRE(v.to_string(3) == "1!11a0.3.4dev+1.2.0");
            REQUIRE(v.to_string(4) == "1!11a0.3.4dev.0+1.2.0.0");
        }

        SECTION("*.1.*")
        {
            const auto v = Version(0, { { { 0, "*" } }, { { 1 } }, { { 0, "*" } } }, {});
            REQUIRE(v.to_string() == "0*.1.0*");
            REQUIRE(v.to_string(1) == "0*");
            REQUIRE(v.to_string(2) == "0*.1");
            REQUIRE(v.to_string(3) == "0*.1.0*");
            REQUIRE(v.to_string(4) == "0*.1.0*.0");
            REQUIRE(v.to_string_glob() == "*.1.*");
        }
    }

    /**
     * Test from Conda
     *
     * @see https://github.com/conda/conda/blob/main/tests/models/test_version.py
     */
    TEST_CASE("Version::parse", "[mamba::specs][mamba::specs::Version]")
    {
        // clang-format off
        auto const sorted_version = std::vector<std::pair<std::string_view, Version>>{
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
            REQUIRE(Version::parse(raw) == expected);
        }

        REQUIRE(
            std::is_sorted(
                sorted_version.cbegin(),
                sorted_version.cend(),
                [](const auto& a, const auto& b) { return a.second < b.second; }
            )
        );

        // Default constructed
        REQUIRE(Version::parse("0.0").value() == Version());

        // Lowercase and strip
        REQUIRE(Version::parse("0.4.1.rc").value() == Version::parse("  0.4.1.RC  "));
        REQUIRE(Version::parse("  0.4.1.RC  ").value() == Version::parse("0.4.1.rc"));

        // Functional assertions
        REQUIRE(Version::parse("  0.4.rc  ").value() == Version::parse("0.4.RC"));
        REQUIRE(Version::parse("0.4").value() == Version::parse("0.4.0"));
        REQUIRE(Version::parse("0.4").value() != Version::parse("0.4.1"));
        REQUIRE(Version::parse("0.4.a1").value() == Version::parse("0.4.0a1"));
        REQUIRE(Version::parse("0.4.a1").value() != Version::parse("0.4.1a1"));

        // Parse implicit zeros
        REQUIRE(Version::parse("0.4.a1").value().version()[2].implicit_leading_zero);
        REQUIRE(Version::parse("0.4.a1").value().to_string() == "0.4.a1");
        REQUIRE(Version::parse("g56ffd88f").value().to_string() == "g56ffd88f");

        // These are valid versions with the special '*' ordering AND they are also used as such
        // with version globs in VersionSpec
        REQUIRE(Version::parse("*") == Version(0, { { { 0, "*" } } }));
        REQUIRE(Version::parse("*.*") == Version(0, { { { 0, "*" } }, { { 0, "*" } } }));
        REQUIRE(
            Version::parse("*.*.*") == Version(0, { { { 0, "*" } }, { { 0, "*" } }, { { 0, "*" } } })
        );
        REQUIRE(
            Version::parse("*.*.2023.12")
            == Version(0, { { { 0, "*" } }, { { 0, "*" } }, { { 2023, "" } }, { { 12, "" } } })
        );
        REQUIRE(Version::parse("1.*") == Version(0, { { { 1, "" } }, { { 0, "*" } } }));
    }

    TEST_CASE("Version::parse negative", "[mamba::specs][mamba::specs::Version]")
    {
        // Wrong epoch
        REQUIRE_FALSE(Version::parse("!1.1").has_value());
        REQUIRE_FALSE(Version::parse("-1!1.1").has_value());
        REQUIRE_FALSE(Version::parse("foo!1.1").has_value());
        REQUIRE_FALSE(Version::parse("0post1!1.1").has_value());

        // Empty parts
        REQUIRE_FALSE(Version::parse("").has_value());
        REQUIRE_FALSE(Version::parse("  ").has_value());
        REQUIRE_FALSE(Version::parse("!2.2").has_value());
        REQUIRE_FALSE(Version::parse("0!").has_value());
        REQUIRE_FALSE(Version::parse("!").has_value());
        REQUIRE_FALSE(Version::parse("1.").has_value());
        REQUIRE_FALSE(Version::parse("1..1").has_value());
        REQUIRE_FALSE(Version::parse("5.5..mw").has_value());
        REQUIRE_FALSE(Version::parse("1.2post+").has_value());
        REQUIRE_FALSE(Version::parse("1!+1.1").has_value());

        // Repeated delimiters
        REQUIRE_FALSE(Version::parse("5.5++").has_value());
        REQUIRE_FALSE(Version::parse("5.5+1+0.0").has_value());
        REQUIRE_FALSE(Version::parse("1!2!3.0").has_value());

        // '-' and '_' delimiters not allowed together.
        REQUIRE_FALSE(Version::parse("1-1_1").has_value());

        // Forbidden characters
        REQUIRE_FALSE(Version::parse("3.5&1").has_value());
        REQUIRE_FALSE(Version::parse("3.5|1").has_value());
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
    TEST_CASE("parse_openssl", "[mamba::specs][mamba::specs::Version]")
    {
        // clang-format off
            auto versions = std::vector{
                Version::parse("1.0.1dev").value(),
                Version::parse("1.0.1_").value(),  // <- this
                Version::parse("1.0.1a").value(),
                Version::parse("1.0.1b").value(),
                Version::parse("1.0.1c").value(),
                Version::parse("1.0.1d").value(),
                Version::parse("1.0.1r").value(),
                Version::parse("1.0.1rc").value(),
                Version::parse("1.0.1rc1").value(),
                Version::parse("1.0.1rc2").value(),
                Version::parse("1.0.1s").value(),
                Version::parse("1.0.1").value(),  // <- compared to this
                Version::parse("1.0.1post.a").value(),
                Version::parse("1.0.1post.b").value(),
                Version::parse("1.0.1post.z").value(),
                Version::parse("1.0.1post.za").value(),
                Version::parse("1.0.2").value(),
            };
        // clang-format on

        // Strict ordering
        REQUIRE(std::is_sorted(versions.cbegin(), versions.cend()));
        // None compare equal (given the is_sorted assumption)
        REQUIRE(std::adjacent_find(versions.cbegin(), versions.cend()) == versions.cend());
    }

    /**
     * Test from Conda slightly modified from the PEP 440 test suite.
     *
     * @see https://github.com/conda/conda/blob/main/tests/models/test_version.py
     * @see https://github.com/pypa/packaging/blob/master/tests/test_version.py
     */
    TEST_CASE("parse_pep440", "[mamba::specs][mamba::specs::Version]")
    {
        auto versions = std::vector{
            // Implicit epoch of 0
            Version::parse("1.0a1").value(),
            Version::parse("1.0a2.dev456").value(),
            Version::parse("1.0a12.dev456").value(),
            Version::parse("1.0a12").value(),
            Version::parse("1.0b1.dev456").value(),
            Version::parse("1.0b2").value(),
            Version::parse("1.0b2.post345.dev456").value(),
            Version::parse("1.0b2.post345").value(),
            Version::parse("1.0c1.dev456").value(),
            Version::parse("1.0c1").value(),
            Version::parse("1.0c3").value(),
            Version::parse("1.0rc2").value(),
            Version::parse("1.0.dev456").value(),
            Version::parse("1.0").value(),
            Version::parse("1.0.post456.dev34").value(),
            Version::parse("1.0.post456").value(),
            Version::parse("1.1.dev1").value(),
            Version::parse("1.2.r32+123456").value(),
            Version::parse("1.2.rev33+123456").value(),
            Version::parse("1.2+abc").value(),
            Version::parse("1.2+abc123def").value(),
            Version::parse("1.2+abc123").value(),
            Version::parse("1.2+123abc").value(),
            Version::parse("1.2+123abc456").value(),
            Version::parse("1.2+1234.abc").value(),
            Version::parse("1.2+123456").value(),
            // Explicit epoch of 1
            Version::parse("1!1.0a1").value(),
            Version::parse("1!1.0a2.dev456").value(),
            Version::parse("1!1.0a12.dev456").value(),
            Version::parse("1!1.0a12").value(),
            Version::parse("1!1.0b1.dev456").value(),
            Version::parse("1!1.0b2").value(),
            Version::parse("1!1.0b2.post345.dev456").value(),
            Version::parse("1!1.0b2.post345").value(),
            Version::parse("1!1.0c1.dev456").value(),
            Version::parse("1!1.0c1").value(),
            Version::parse("1!1.0c3").value(),
            Version::parse("1!1.0rc2").value(),
            Version::parse("1!1.0.dev456").value(),
            Version::parse("1!1.0").value(),
            Version::parse("1!1.0.post456.dev34").value(),
            Version::parse("1!1.0.post456").value(),
            Version::parse("1!1.1.dev1").value(),
            Version::parse("1!1.2.r32+123456").value(),
            Version::parse("1!1.2.rev33+123456").value(),
            Version::parse("1!1.2+abc").value(),
            Version::parse("1!1.2+abc123def").value(),
            Version::parse("1!1.2+abc123").value(),
            Version::parse("1!1.2+123abc").value(),
            Version::parse("1!1.2+123abc456").value(),
            Version::parse("1!1.2+1234.abc").value(),
            Version::parse("1!1.2+123456").value(),
        };
        // clang-format on

        // Strict ordering
        REQUIRE(std::is_sorted(versions.cbegin(), versions.cend()));
        // None compare equal (given the is_sorted assumption)
        REQUIRE(std::adjacent_find(versions.cbegin(), versions.cend()) == versions.cend());
    }
}
