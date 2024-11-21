// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <catch2/catch_all.hpp>

#include "mamba/util/build.hpp"
#include "mamba/util/path_manip.hpp"

using namespace mamba::util;

namespace
{
    TEST_CASE("is_explicit_path")
    {
        REQUIRE(is_explicit_path("."));
        REQUIRE(is_explicit_path("./"));
        REQUIRE(is_explicit_path("./folder/file.txt"));
        REQUIRE(is_explicit_path(".."));
        REQUIRE(is_explicit_path("../file.txt"));
        REQUIRE(is_explicit_path("~"));
        REQUIRE(is_explicit_path("~/there"));
        REQUIRE(is_explicit_path("/"));
        REQUIRE(is_explicit_path("/asset"));

        REQUIRE_FALSE(is_explicit_path(""));
        REQUIRE_FALSE(is_explicit_path("name"));
        REQUIRE_FALSE(is_explicit_path("folder/file.txt"));
        REQUIRE_FALSE(is_explicit_path("file://makefile"));
    }

    TEST_CASE("path_has_drive_letter")
    {
        REQUIRE(path_has_drive_letter("C:/folder/file"));
        CHECK_EQ(path_get_drive_letter("C:/folder/file"), 'C');
        REQUIRE(path_has_drive_letter(R"(C:\folder\file)"));
        CHECK_EQ(path_get_drive_letter(R"(C:\folder\file)"), 'C');
        REQUIRE_FALSE(path_has_drive_letter("/folder/file"));
        REQUIRE_FALSE(path_has_drive_letter("folder/file"));
        REQUIRE_FALSE(path_has_drive_letter(R"(\folder\file)"));
        REQUIRE_FALSE(path_has_drive_letter(R"(folder\file)"));
    }

    TEST_CASE("path_win_detect_sep")
    {
        CHECK_EQ(path_win_detect_sep("file"), std::nullopt);

        CHECK_EQ(path_win_detect_sep("C:/file"), '/');
        CHECK_EQ(path_win_detect_sep("~/file"), '/');
        CHECK_EQ(path_win_detect_sep("/folder/file"), '/');

        CHECK_EQ(path_win_detect_sep(R"(C:\file)"), '\\');
        CHECK_EQ(path_win_detect_sep(R"(~\file)"), '\\');
        CHECK_EQ(path_win_detect_sep(R"(\\folder\\file)"), '\\');
    }

    TEST_CASE("path_win_to_posix")
    {
        CHECK_EQ(path_win_to_posix(""), "");
        CHECK_EQ(path_win_to_posix("file"), "file");
        CHECK_EQ(path_win_to_posix(R"(C:\folder\file)"), "C:/folder/file");
        CHECK_EQ(path_win_to_posix("C:/folder/file"), "C:/folder/file");
    }

    TEST_CASE("path_win_to_posix")
    {
        CHECK_EQ(path_posix_to_win(""), "");
        CHECK_EQ(path_posix_to_win("file"), "file");
        CHECK_EQ(path_posix_to_win("C:/folder/file"), R"(C:\folder\file)");
        CHECK_EQ(path_posix_to_win(R"(C:\folder\file)"), R"(C:\folder\file)");
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

    TEST_CASE("path_is_prefix")
    {
        REQUIRE(path_is_prefix("", ""));
        REQUIRE(path_is_prefix("", "folder"));

        REQUIRE(path_is_prefix("folder", "folder"));
        REQUIRE(path_is_prefix("/", "/folder"));
        REQUIRE(path_is_prefix("/folder", "/folder"));
        REQUIRE(path_is_prefix("/folder/", "/folder/"));
        REQUIRE(path_is_prefix("/folder", "/folder/"));
        REQUIRE(path_is_prefix("/folder/", "/folder/"));
        REQUIRE(path_is_prefix("/folder", "/folder/file.txt"));
        REQUIRE(path_is_prefix("/folder/", "/folder/file.txt"));
        REQUIRE(path_is_prefix("/folder", "/folder/more/file.txt"));
        REQUIRE(path_is_prefix("/folder/", "/folder/more/file.txt"));
        REQUIRE(path_is_prefix("/folder/file.txt", "/folder/file.txt"));
        REQUIRE(path_is_prefix("folder/file.txt", "folder/file.txt"));

        REQUIRE_FALSE(path_is_prefix("/folder", "/"));
        REQUIRE_FALSE(path_is_prefix("/folder/file", "/folder"));
        REQUIRE_FALSE(path_is_prefix("/folder", "/folder-more"));
        REQUIRE_FALSE(path_is_prefix("/folder/file.json", "/folder/file.txt"));
        REQUIRE_FALSE(path_is_prefix("folder/file.json", "folder/file.txt"));

        // Debatable "folder/" interpreted as ["folder", ""] in term of splits.
        REQUIRE_FALSE(path_is_prefix("folder/", "folder"));
        REQUIRE_FALSE(path_is_prefix("/folder/", "/folder"));
    }

    TEST_CASE("path_concat")
    {
        SECTION("proper concatenation")
        {
            CHECK_EQ(path_concat("", "file", '/'), "file");
            CHECK_EQ(path_concat("some/folder", "", '/'), "some/folder");

            CHECK_EQ(path_concat("some/folder", "file", '/'), "some/folder/file");
            CHECK_EQ(path_concat("some/folder/", "file", '/'), "some/folder/file");
            CHECK_EQ(path_concat("some/folder", "/file", '/'), "some/folder/file");
            CHECK_EQ(path_concat("some/folder/", "/file", '/'), "some/folder/file");
        }

        SECTION("Separator detection")
        {
            CHECK_EQ(path_concat("some/folder", "file"), "some/folder/file");
            if (on_win)
            {
                CHECK_EQ(path_concat(R"(D:\some\folder)", "file"), R"(D:\some\folder\file)");
            }
        }
    }

    TEST_CASE("expand_home")
    {
        CHECK_EQ(expand_home("", ""), "");
        CHECK_EQ(expand_home("~", ""), "");
        CHECK_EQ(expand_home("", "/user/mamba"), "");
        CHECK_EQ(expand_home("~", "/user/mamba"), "/user/mamba");
        CHECK_EQ(expand_home("~/", "/user/mamba"), "/user/mamba/");
        CHECK_EQ(expand_home("~/folder", "/user/mamba"), "/user/mamba/folder");
        CHECK_EQ(expand_home("~/folder", "/user/mamba/"), "/user/mamba/folder");
        CHECK_EQ(expand_home("file~name", "/user/mamba"), "file~name");
        CHECK_EQ(expand_home("~file", "/user/mamba"), "~file");
        CHECK_EQ(expand_home("~/foo", "."), "./foo");
    }

    TEST_CASE("shrink_home")
    {
        CHECK_EQ(shrink_home("", ""), "");
        CHECK_EQ(shrink_home("/user/mamba", ""), "/user/mamba");
        CHECK_EQ(shrink_home("/user/mamba", "/user/mamba"), "~");
        CHECK_EQ(shrink_home("/user/mamba/", "/user/mamba"), "~/");  // Final "/" as in first input
        CHECK_EQ(shrink_home("/user/mamba", "/user/mamba/"), "~");
        CHECK_EQ(shrink_home("/user/mamba/", "/user/mamba/"), "~/");  // Final "/" as in first input
        CHECK_EQ(shrink_home("/user/mamba/file", "/user/mamba"), "~/file");
        CHECK_EQ(shrink_home("/user/mamba/file", "/user/mamba/"), "~/file");
        CHECK_EQ(shrink_home("/user/mamba-dev/file", "/user/mamba"), "/user/mamba-dev/file");
    }
}
