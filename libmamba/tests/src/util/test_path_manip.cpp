// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <doctest/doctest.h>

#include "mamba/util/build.hpp"
#include "mamba/util/path_manip.hpp"

using namespace mamba::util;

TEST_SUITE("util::path_manip")
{
    TEST_CASE("is_explicit_path")
    {
        CHECK(is_explicit_path("."));
        CHECK(is_explicit_path("./"));
        CHECK(is_explicit_path("./folder/file.txt"));
        CHECK(is_explicit_path(".."));
        CHECK(is_explicit_path("../file.txt"));
        CHECK(is_explicit_path("~"));
        CHECK(is_explicit_path("~/there"));
        CHECK(is_explicit_path("/"));
        CHECK(is_explicit_path("/asset"));

        CHECK_FALSE(is_explicit_path(""));
        CHECK_FALSE(is_explicit_path("name"));
        CHECK_FALSE(is_explicit_path("folder/file.txt"));
        CHECK_FALSE(is_explicit_path("file://makefile"));
    }

    TEST_CASE("path_has_drive_letter")
    {
        CHECK(path_has_drive_letter("C:/folder/file"));
        CHECK_EQ(path_get_drive_letter("C:/folder/file"), 'C');
        CHECK(path_has_drive_letter(R"(C:\folder\file)"));
        CHECK_EQ(path_get_drive_letter(R"(C:\folder\file)"), 'C');
        CHECK_FALSE(path_has_drive_letter("/folder/file"));
        CHECK_FALSE(path_has_drive_letter("folder/file"));
        CHECK_FALSE(path_has_drive_letter(R"(\folder\file)"));
        CHECK_FALSE(path_has_drive_letter(R"(folder\file)"));
    }

    TEST_CASE("path_win_to_posix")
    {
        CHECK_EQ(path_win_to_posix(""), "");
        CHECK_EQ(path_win_to_posix("file"), "file");
        CHECK_EQ(path_win_to_posix(R"(C:\folder\file)"), "C:/folder/file");
        CHECK_EQ(path_win_to_posix("C:/folder/file"), "C:/folder/file");
    }

    TEST_CASE("path_to_posix")
    {
        CHECK_EQ(path_to_posix(""), "");
        CHECK_EQ(path_to_posix("file"), "file");
        CHECK_EQ(path_to_posix("folder/file"), "folder/file");
        CHECK_EQ(path_to_posix("/folder/file"), "/folder/file");

        if (on_win)
        {
            CHECK_EQ(path_to_posix(R"(C:\folder\file)"), "C:/folder/file");
            CHECK_EQ(path_to_posix("C:/folder/file"), "C:/folder/file");
        }
        else
        {
            CHECK_EQ(path_to_posix(R"(folder/weird\file)"), R"(folder/weird\file)");
        }
    }
}
