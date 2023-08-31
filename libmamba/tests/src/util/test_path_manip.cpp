// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <doctest/doctest.h>

#include "mamba/util/path_manip.hpp"

using namespace mamba::util;

TEST_SUITE("util::path_manip")
{
    TEST_CASE("path_win_to_posix")
    {
        CHECK_EQ(path_win_to_posix(""), "");
        CHECK_EQ(path_win_to_posix("file"), "file");
        CHECK_EQ(path_win_to_posix(R"(C:\folder\file)"), "C:/folder/file");
        CHECK_EQ(path_win_to_posix("C:/folder/file"), "C:/folder/file");
    }
}
