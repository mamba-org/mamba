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
}
