// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "mamba/fs/filesystem.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

using namespace mamba;
using namespace mamba::util;

TEST_SUITE("util::url_manip")
{
    TEST_CASE("abs_path_to_url")
    {
        SUBCASE("/users/test/miniconda3")
        {
            CHECK_EQ(abs_path_to_url("/users/test/miniconda3"), "file:///users/test/miniconda3");
        }

        SUBCASE(R"(D:\users\test\miniconda3)")
        {
            if (on_win)
            {
                CHECK_EQ(
                    abs_path_to_url(R"(D:\users\test\miniconda3)"),
                    "file://D:/users/test/miniconda3"
                );
            }
        }

        SUBCASE("/tmp/foo bar")
        {
            CHECK_EQ(abs_path_to_url("/tmp/foo bar"), "file:///tmp/foo%20bar");
        }
    }

    TEST_CASE("abs_path_or_url_to_url")
    {
        SUBCASE("/users/test/miniconda3")
        {
            CHECK_EQ(abs_path_or_url_to_url("/users/test/miniconda3"), "file:///users/test/miniconda3");
        }

        SUBCASE("file:///tmp/bar")
        {
            CHECK_EQ(abs_path_or_url_to_url("file:///tmp/bar"), "file:///tmp/bar");
        }
    }

    TEST_CASE("path_to_url")
    {
        const std::string win_drive = fs::absolute(fs::u8path("/")).string().substr(0, 1);

        SUBCASE("/users/test/miniconda3")
        {
            auto url = path_to_url("/users/test/miniconda3");
            if (on_win)
            {
                CHECK_EQ(url, concat("file://", win_drive, ":/users/test/miniconda3"));
            }
            else
            {
                CHECK_EQ(url, "file:///users/test/miniconda3");
            }
        }

        SUBCASE(R"(D:\users\test\miniconda3)")
        {
            if (on_win)
            {
                CHECK_EQ(path_to_url(R"(D:\users\test\miniconda3)"), "file://D:/users/test/miniconda3");
            }
        }

        SUBCASE("/tmp/foo bar")
        {
            auto url = path_to_url("/tmp/foo bar");
            if (on_win)
            {
                CHECK_EQ(url, concat("file://", win_drive, ":/tmp/foo%20bar"));
            }
            else
            {
                CHECK_EQ(url, "file:///tmp/foo%20bar");
            }
        }

        SUBCASE("./folder/./../folder")
        {
            auto url = path_to_url("./folder/./../folder");
            if (on_win)
            {
                CHECK(starts_with(url, concat("file://", win_drive, ":/")));
                CHECK(ends_with(url, "/folder"));
            }
            else
            {
                const auto expected_folder = fs::absolute("folder").lexically_normal();
                CHECK_EQ(url, concat("file://", expected_folder.string()));
            }
        }
    }

    TEST_CASE("path_or_url_to_url")
    {
        const std::string win_drive = fs::absolute(fs::u8path("/")).string().substr(0, 1);

        SUBCASE("/tmp/foo bar")
        {
            auto url = path_or_url_to_url("/tmp/foo bar");
            if (on_win)
            {
                CHECK_EQ(url, concat("file://", win_drive, ":/tmp/foo%20bar"));
            }
            else
            {
                CHECK_EQ(url, "file:///tmp/foo%20bar");
            }
        }

        SUBCASE("file:///tmp/bar")
        {
            CHECK_EQ(path_or_url_to_url("file:///tmp/bar"), "file:///tmp/bar");
        }
    }

    TEST_CASE("url_concat")
    {
        CHECK_EQ(url_concat("", ""), "");
        CHECK_EQ(url_concat("", "/"), "/");
        CHECK_EQ(url_concat("/", ""), "/");
        CHECK_EQ(url_concat("/", "/"), "/");

        CHECK_EQ(url_concat("mamba.org", "folder"), "mamba.org/folder");
        CHECK_EQ(url_concat("mamba.org", "/folder"), "mamba.org/folder");
        CHECK_EQ(url_concat("mamba.org/", "folder"), "mamba.org/folder");
        CHECK_EQ(url_concat("mamba.org/", "/folder"), "mamba.org/folder");

        CHECK_EQ(
            url_concat("mamba.org", 't', std::string("/sometoken/"), std::string_view("conda-forge")),
            "mamba.org/t/sometoken/conda-forge"
        );
    }

    TEST_CASE("file_uri_unc2_to_unc4")
    {
        for (const std::string uri : {
                 "http://example.com/test",
                 R"(file://C:/Program\ (x74)/Users/hello\ world)",
                 R"(file:///C:/Program\ (x74)/Users/hello\ world)",
                 "file:////server/share",
                 "file:///path/to/data.xml",
                 "file:///absolute/path",
                 R"(file://\\server\path)",
             })
        {
            CAPTURE(uri);
            CHECK_EQ(file_uri_unc2_to_unc4(uri), uri);
        }
        CHECK_EQ(file_uri_unc2_to_unc4("file://server/share"), "file:////server/share");
        CHECK_EQ(file_uri_unc2_to_unc4("file://server"), "file:////server");
    }

    TEST_CASE("url_get_scheme")
    {
        CHECK_EQ(url_get_scheme("http://mamba.org"), "http");
        CHECK_EQ(url_get_scheme("file:///folder/file.txt"), "file");
        CHECK_EQ(url_get_scheme("s3://bucket/file.txt"), "s3");
        CHECK_EQ(url_get_scheme("mamba.org"), "");
        CHECK_EQ(url_get_scheme("://"), "");
        CHECK_EQ(url_get_scheme("f#gre://"), "");
        CHECK_EQ(url_get_scheme(""), "");
    }

    TEST_CASE("url_has_scheme")
    {
        CHECK(url_has_scheme("http://mamba.org"));
        CHECK(url_has_scheme("file:///folder/file.txt"));
        CHECK(url_has_scheme("s3://bucket/file.txt"));
        CHECK_FALSE(url_has_scheme("mamba.org"));
        CHECK_FALSE(url_has_scheme("://"));
        CHECK_FALSE(url_has_scheme("f#gre://"));
        CHECK_FALSE(url_has_scheme(""));
    }
}
