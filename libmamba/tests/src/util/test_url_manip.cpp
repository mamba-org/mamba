// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <string_view>

#include <catch2/catch_all.hpp>

#include "mamba/fs/filesystem.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

using namespace mamba;
using namespace mamba::util;

TEST_CASE("abs_path_to_url")
{
    SECTION("/users/test/miniconda3")
    {
        REQUIRE(abs_path_to_url("/users/test/miniconda3") == "file:///users/test/miniconda3");
    }

    SECTION(R"(D:\users\test\miniconda3)")
    {
        if (on_win)
        {
            REQUIRE(
                abs_path_to_url(R"(D:\users\test\miniconda3)") == "file://D:/users/test/miniconda3"
            );
        }
    }

    SECTION("/tmp/foo bar")
    {
        REQUIRE(abs_path_to_url("/tmp/foo bar") == "file:///tmp/foo%20bar");
    }
}

TEST_CASE("abs_path_or_url_to_url")
{
    SECTION("/users/test/miniconda3")
    {
        REQUIRE(abs_path_or_url_to_url("/users/test/miniconda3") == "file:///users/test/miniconda3");
    }

    SECTION("file:///tmp/bar")
    {
        REQUIRE(abs_path_or_url_to_url("file:///tmp/bar") == "file:///tmp/bar");
    }
}

TEST_CASE("path_to_url")
{
    const std::string win_drive = fs::absolute(fs::u8path("/")).string().substr(0, 1);

    SECTION("/users/test/miniconda3")
    {
        auto url = path_to_url("/users/test/miniconda3");
        if (on_win)
        {
            REQUIRE(url == concat("file://", win_drive, ":/users/test/miniconda3"));
        }
        else
        {
            REQUIRE(url == "file:///users/test/miniconda3");
        }
    }

    SECTION(R"(D:\users\test\miniconda3)")
    {
        if (on_win)
        {
            REQUIRE(path_to_url(R"(D:\users\test\miniconda3)") == "file://D:/users/test/miniconda3");
        }
    }

    SECTION("/tmp/foo bar")
    {
        auto url = path_to_url("/tmp/foo bar");
        if (on_win)
        {
            REQUIRE(url == concat("file://", win_drive, ":/tmp/foo%20bar"));
        }
        else
        {
            REQUIRE(url == "file:///tmp/foo%20bar");
        }
    }

    SECTION("./folder/./../folder")
    {
        auto url = path_to_url("./folder/./../folder");
        if (on_win)
        {
            REQUIRE(starts_with(url, concat("file://", win_drive, ":/")));
            REQUIRE(ends_with(url, "/folder"));
        }
        else
        {
            const auto expected_folder = fs::absolute("folder").lexically_normal();
            REQUIRE(url == concat("file://", expected_folder.string()));
        }
    }
}

TEST_CASE("path_or_url_to_url")
{
    const std::string win_drive = fs::absolute(fs::u8path("/")).string().substr(0, 1);

    SECTION("/tmp/foo bar")
    {
        auto url = path_or_url_to_url("/tmp/foo bar");
        if (on_win)
        {
            REQUIRE(url == concat("file://", win_drive, ":/tmp/foo%20bar"));
        }
        else
        {
            REQUIRE(url == "file:///tmp/foo%20bar");
        }
    }

    SECTION("file:///tmp/bar")
    {
        REQUIRE(path_or_url_to_url("file:///tmp/bar") == "file:///tmp/bar");
    }
}

TEST_CASE("url_concat")
{
    REQUIRE(url_concat("", "") == "");
    REQUIRE(url_concat("", "/") == "/");
    REQUIRE(url_concat("/", "") == "/");
    REQUIRE(url_concat("/", "/") == "/");

    REQUIRE(url_concat("mamba.org", "folder") == "mamba.org/folder");
    REQUIRE(url_concat("mamba.org", "/folder") == "mamba.org/folder");
    REQUIRE(url_concat("mamba.org/", "folder") == "mamba.org/folder");
    REQUIRE(url_concat("mamba.org/", "/folder") == "mamba.org/folder");

    REQUIRE(
        url_concat("mamba.org", 't', std::string("/sometoken/"), std::string_view("conda-forge"))
        == "mamba.org/t/sometoken/conda-forge"
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
        REQUIRE(file_uri_unc2_to_unc4(uri) == uri);
    }
    REQUIRE(file_uri_unc2_to_unc4("file://server/share") == "file:////server/share");
    REQUIRE(file_uri_unc2_to_unc4("file://server") == "file:////server");
}

TEST_CASE("url_get_scheme")
{
    REQUIRE(url_get_scheme("http://mamba.org") == "http");
    REQUIRE(url_get_scheme("file:///folder/file.txt") == "file");
    REQUIRE(url_get_scheme("s3://bucket/file.txt") == "s3");
    REQUIRE(url_get_scheme("mamba.org") == "");
    REQUIRE(url_get_scheme("://") == "");
    REQUIRE(url_get_scheme("f#gre://") == "");
    REQUIRE(url_get_scheme("") == "");
}

TEST_CASE("url_has_scheme")
{
    REQUIRE(url_has_scheme("http://mamba.org"));
    REQUIRE(url_has_scheme("file:///folder/file.txt"));
    REQUIRE(url_has_scheme("s3://bucket/file.txt"));
    REQUIRE_FALSE(url_has_scheme("mamba.org"));
    REQUIRE_FALSE(url_has_scheme("://"));
    REQUIRE_FALSE(url_has_scheme("f#gre://"));
    REQUIRE_FALSE(url_has_scheme(""));
}
