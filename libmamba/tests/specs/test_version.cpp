// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "mamba/specs/version.hpp"

namespace mamba::specs
{

    TEST(version, atom_comparison)
    {
        // No litteral
        EXPECT_EQ(VersionPartAtom(1), VersionPartAtom(1, ""));
        // lowercase
        EXPECT_EQ(VersionPartAtom(1, "dev"), VersionPartAtom(1, "DEV"));
        // All operator comparison for mumerals
        EXPECT_NE(VersionPartAtom(1), VersionPartAtom(2, "dev"));
        EXPECT_LT(VersionPartAtom(1), VersionPartAtom(2, "dev"));
        EXPECT_LE(VersionPartAtom(1), VersionPartAtom(2, "dev"));
        EXPECT_GT(VersionPartAtom(2, "dev"), VersionPartAtom(1));
        EXPECT_GE(VersionPartAtom(2, "dev"), VersionPartAtom(1));
        // All operator comparison for litterals
        EXPECT_NE(VersionPartAtom(1, "dev"), VersionPartAtom(1, "a"));
        EXPECT_LT(VersionPartAtom(1, "dev"), VersionPartAtom(1, "a"));
        EXPECT_LE(VersionPartAtom(1, "dev"), VersionPartAtom(1, "a"));
        EXPECT_GT(VersionPartAtom(1, "a"), VersionPartAtom(1, "dev"));
        EXPECT_GE(VersionPartAtom(1, "a"), VersionPartAtom(1, "dev"));

        // clang-format off
        auto sorted_atoms = std::vector<VersionPartAtom>{
            { 1, "*" },
            { 1, "dev" },
            { 1, "_" },
            { 1, "a" },
            { 1, "alpha" },
            { 1, "b" },
            { 1, "beta" },
            { 1, "c" },
            { 1, "r" },
            { 1, "rc" },
            { 1, "" },
            { 1, "post" },
        };
        // clang-format on

        // Strict ordering
        EXPECT_TRUE(std::is_sorted(sorted_atoms.cbegin(), sorted_atoms.cend()));
        // None compare equal (given the is_sorted assumption)
        EXPECT_EQ(std::adjacent_find(sorted_atoms.cbegin(), sorted_atoms.cend()), sorted_atoms.cend());
    }

    TEST(version, atom_format)
    {
        EXPECT_EQ(VersionPartAtom(1, "dev").str(), "1dev");
        EXPECT_EQ(VersionPartAtom(2).str(), "2");
    }

    TEST(version, version_comparison)
    {
        auto v = Version(0, { { { 1, "post" } } });
        ASSERT_EQ(v.version().size(), 1);
        ASSERT_EQ(v.version().front().size(), 1);
        ASSERT_EQ(v.version().front().front(), VersionPartAtom(1, "post"));

        // Same empty 0!1post version
        EXPECT_EQ(Version(0, { { { 1, "post" } } }), Version(0, { { { 1, "post" } } }));
        // Empty trailing atom 0!1a == 0!1a0""
        EXPECT_EQ(Version(0, { { { 1, "a" } } }), Version(0, { { { 1, "a" }, {} } }));
        // Empty trailing part 0!1a == 0!1a.0""
        EXPECT_EQ(Version(0, { { { 1, "a" } } }), Version(0, { { { 1, "a" } }, { {} } }));
        // Mixed 0!1a0""0"" == 0!1a.0""
        EXPECT_EQ(Version(0, { { { 1, "a" }, {}, {} } }), Version(0, { { { 1, "a" } }, { {} } }));

        // Different epoch 0!2post < 1!1dev
        EXPECT_LT(Version(0, { { { 2, "post" } } }), Version(1, { { { 1, "dev" } } }));
        EXPECT_GE(Version(1, { { { 1, "dev" } } }), Version(0, { { { 2, "post" } } }));
        // Different lenght with dev
        EXPECT_LT(Version(0, { { { 1 } }, { { 0, "dev" } } }), Version(0, { { { 1 } } }));
        EXPECT_LT(Version(0, { { { 1 } }, { { 0 } }, { { 0, "dev" } } }), Version(0, { { { 1 } } }));
        // Different major 0!1post < 0!2dev
        EXPECT_LT(Version(0, { { { 1, "post" } } }), Version(0, { { { 2, "dev" } } }));
        // Different length 0!2"".0"" < 0!11"".0"".0post all operator
        EXPECT_NE(Version(0, { { { 2 }, { 0 } } }), Version(0, { { { 11 }, { 0 }, { 0, "post" } } }));
        EXPECT_LT(Version(0, { { { 2 }, { 0 } } }), Version(0, { { { 11 }, { 0 }, { 0, "post" } } }));
        EXPECT_LE(Version(0, { { { 2 }, { 0 } } }), Version(0, { { { 11 }, { 0 }, { 0, "post" } } }));
        EXPECT_GT(Version(0, { { { 11 }, { 0 }, { 0, "post" } } }), Version(0, { { { 2 }, { 0 } } }));
        EXPECT_GE(Version(0, { { { 11 }, { 0 }, { 0, "post" } } }), Version(0, { { { 2 }, { 0 } } }));
    }

    TEST(version, version_format)
    {
        // clang-format off
        EXPECT_EQ(
            Version(0, {{{11, "a"}, {0, "post"}}, {{3}}, {{4, "dev"}}}).str(),
            "11a0post.3.4dev"
        );
        EXPECT_EQ(
            Version(1, {{{11, "a"}, {0}}, {{3}}, {{4, "dev"}}}).str(),
            "1!11a0.3.4dev"
        );
        EXPECT_EQ(
            Version(1, {{{11, "a"}, {0}}, {{3}}, {{4, "dev"}}}, {{{1}}, {{2}}}).str(),
            "1!11a0.3.4dev+1.2"
        );
        // clang-format on
    }
}
