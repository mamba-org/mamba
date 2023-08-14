// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "mamba/util/url.hpp"

using namespace mamba::util;

TEST_SUITE("util::URL")
{
    TEST_CASE("URL builder")
    {
        SUBCASE("Empty")
        {
            URL url{};
            CHECK_EQ(url.scheme(), URL::https);
            CHECK_EQ(url.host(), URL::localhost);
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.pretty_path(), "/");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
        }

        SUBCASE("Complete")
        {
            URL url{};
            url.set_scheme("https")
                .set_host("mamba.org")
                .set_user("user")
                .set_password("password")
                .set_port("8080")
                .set_path("/folder/file.html")
                .set_query("param=value")
                .set_fragment("fragment");
            CHECK_EQ(url.scheme(), "https");
            CHECK_EQ(url.host(), "mamba.org");
            CHECK_EQ(url.user(), "user");
            CHECK_EQ(url.password(), "password");
            CHECK_EQ(url.port(), "8080");
            CHECK_EQ(url.path(), "/folder/file.html");
            CHECK_EQ(url.pretty_path(), "/folder/file.html");
            CHECK_EQ(url.query(), "param=value");
            CHECK_EQ(url.fragment(), "fragment");
        }

        SUBCASE("Path")
        {
            URL url{};
            url.set_path("path/");
            CHECK_EQ(url.path(), "/path/");
            CHECK_EQ(url.pretty_path(), "/path/");
        }

        SUBCASE("Windows path")
        {
            URL url{};
            url.set_scheme("file").set_path("C:/folder/file.txt");
            CHECK_EQ(url.path(), "/C:/folder/file.txt");
            CHECK_EQ(url.pretty_path(), "C:/folder/file.txt");
        }

        SUBCASE("Case")
        {
            URL url{};
            url.set_scheme("FtP").set_host("sOme_Host.COM");
            CHECK_EQ(url.scheme(), "ftp");
            CHECK_EQ(url.host(), "some_host.com");
        }

        SUBCASE("Invalid")
        {
            URL url{};
            CHECK_THROWS_AS(url.set_scheme(""), std::invalid_argument);
            CHECK_THROWS_AS(url.set_host(""), std::invalid_argument);
            CHECK_THROWS_AS(url.set_port("not-a-number"), std::invalid_argument);
        }

        SUBCASE("Encoding")
        {
            URL url{};
            url.set_user("micro@mamba.pm", URL::Encode::yes);
            CHECK_EQ(url.user(URL::Decode::no), "micro%40mamba.pm");
            CHECK_EQ(url.user(URL::Decode::yes), "micro@mamba.pm");
            url.set_password(R"(#!$&'"ab23)", URL::Encode::yes);
            CHECK_EQ(url.password(URL::Decode::no), "%23%21%24%26%27%22ab23");
            CHECK_EQ(url.password(URL::Decode::yes), R"(#!$&'"ab23)");
        }
    }

    TEST_CASE("parse")
    {
        SUBCASE("Empty")
        {
            const URL url = URL::parse("");
            CHECK_EQ(url.scheme(), URL::https);
            CHECK_EQ(url.host(), URL::localhost);
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.pretty_path(), "/");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SUBCASE("mamba.org")
        {
            const URL url = URL::parse("mamba.org");
            CHECK_EQ(url.scheme(), URL::https);
            CHECK_EQ(url.host(), "mamba.org");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.pretty_path(), "/");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SUBCASE("http://mamba.org")
        {
            const URL url = URL::parse("http://mamba.org");
            CHECK_EQ(url.scheme(), "http");
            CHECK_EQ(url.host(), "mamba.org");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.pretty_path(), "/");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SUBCASE("s3://userx123:üúßsajd@mamba.org")
        {
            const URL url = URL::parse("s3://userx123:üúßsajd@mamba.org");
            CHECK_EQ(url.scheme(), "s3");
            CHECK_EQ(url.host(), "mamba.org");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.pretty_path(), "/");
            CHECK_EQ(url.user(), "userx123");
            CHECK_EQ(url.password(), "üúßsajd");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SUBCASE("http://user%40email.com:test@localhost:8000")
        {
            const URL url = URL::parse("http://user%40email.com:test@localhost:8000");
            CHECK_EQ(url.scheme(), "http");
            CHECK_EQ(url.host(), "localhost");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.pretty_path(), "/");
            CHECK_EQ(url.user(), "user@email.com");
            CHECK_EQ(url.password(), "test");
            CHECK_EQ(url.port(), "8000");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SUBCASE("http://:pass@localhost:8000")
        {
            const URL url = URL::parse("http://:pass@localhost:8000");
            CHECK_EQ(url.scheme(), "http");
            CHECK_EQ(url.host(), "localhost");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.pretty_path(), "/");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "pass");
            CHECK_EQ(url.port(), "8000");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SUBCASE("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333")
        {
            const URL url = URL::parse("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333");
            CHECK_EQ(url.scheme(), "https");
            CHECK_EQ(url.host(), "mamba🆒🔬.org");
            CHECK_EQ(url.path(), "/this/is/a/path/");
            CHECK_EQ(url.pretty_path(), "/this/is/a/path/");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "query=123&xyz=3333");
            CHECK_EQ(url.fragment(), "");
        }

        SUBCASE("file://C:/Users/wolfv/test/document.json")
        {
#ifdef _WIN32
            const URL url = URL::parse("file://C:/Users/wolfv/test/document.json");
            CHECK_EQ(url.scheme(), "file");
            CHECK_EQ(url.host(), URL::localhost);
            CHECK_EQ(url.path(), "/C:/Users/wolfv/test/document.json");
            CHECK_EQ(url.pretty_path(), "C:/Users/wolfv/test/document.json");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
#endif
        }

        SUBCASE("file:///home/wolfv/test/document.json")
        {
#ifndef _WIN32
            const URL url = URL::parse("file:///home/wolfv/test/document.json");
            CHECK_EQ(url.scheme(), "file");
            CHECK_EQ(url.host(), URL::localhost);
            CHECK_EQ(url.path(), "/home/wolfv/test/document.json");
            CHECK_EQ(url.pretty_path(), "/home/wolfv/test/document.json");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
#endif
        }

        SUBCASE("https://169.254.0.0/page")
        {
            const URL url = URL::parse("https://169.254.0.0/page");
            CHECK_EQ(url.scheme(), "https");
            CHECK_EQ(url.host(), "169.254.0.0");
            CHECK_EQ(url.path(), "/page");
            CHECK_EQ(url.pretty_path(), "/page");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SUBCASE("ftp://user:pass@[2001:db8:85a3:8d3:1319:0:370:7348]:9999/page")
        {
            const URL url = URL::parse("ftp://user:pass@[2001:db8:85a3:8d3:1319:0:370:7348]:9999/page"
            );
            CHECK_EQ(url.scheme(), "ftp");
            CHECK_EQ(url.host(), "[2001:db8:85a3:8d3:1319:0:370:7348]");
            CHECK_EQ(url.path(), "/page");
            CHECK_EQ(url.pretty_path(), "/page");
            CHECK_EQ(url.user(), "user");
            CHECK_EQ(url.password(), "pass");
            CHECK_EQ(url.port(), "9999");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }
    }

    TEST_CASE("str")
    {
        SUBCASE("scheme option")
        {
            URL url = {};
            url.set_host("mamba.org");

            SUBCASE("defaut scheme")
            {
                CHECK_EQ(url.str(URL::StripScheme::no), "https://mamba.org/");
                CHECK_EQ(url.str(URL::StripScheme::yes), "mamba.org/");
            }

            SUBCASE("ftp scheme")
            {
                url.set_scheme("ftp");
                CHECK_EQ(url.str(URL::StripScheme::no), "ftp://mamba.org/");
                CHECK_EQ(url.str(URL::StripScheme::yes), "mamba.org/");
            }
        }

        SUBCASE("https://user:password@mamba.org:8080/folder/file.html?param=value#fragment")
        {
            URL url{};
            url.set_scheme("https")
                .set_host("mamba.org")
                .set_user("user")
                .set_password("password")
                .set_port("8080")
                .set_path("/folder/file.html")
                .set_query("param=value")
                .set_fragment("fragment");

            CHECK_EQ(
                url.str(),
                "https://user:password@mamba.org:8080/folder/file.html?param=value#fragment"
            );
        }

        SUBCASE("user@mamba.org")
        {
            URL url{};
            url.set_host("mamba.org").set_user("user");
            CHECK_EQ(url.str(), "https://user@mamba.org/");
            CHECK_EQ(url.str(URL::StripScheme::yes), "user@mamba.org/");
        }

        SUBCASE("https://mamba.org")
        {
            URL url{};
            url.set_scheme("https").set_host("mamba.org");
            CHECK_EQ(url.str(), "https://mamba.org/");
            CHECK_EQ(url.str(URL::StripScheme::yes), "mamba.org/");
        }

        SUBCASE("file:////folder/file.txt")
        {
            URL url{};
            url.set_scheme("file").set_path("//folder/file.txt");
            CHECK_EQ(url.str(), "file:////folder/file.txt");
            CHECK_EQ(url.str(URL::StripScheme::yes), "//folder/file.txt");
        }

        SUBCASE("file:///folder/file.txt")
        {
            URL url{};
            url.set_scheme("file").set_path("/folder/file.txt");
            CHECK_EQ(url.str(), "file:///folder/file.txt");
            CHECK_EQ(url.str(URL::StripScheme::yes), "/folder/file.txt");
        }

        SUBCASE("file:///C:/folder/file.txt")
        {
            URL url{};
            url.set_scheme("file").set_path("C:/folder/file.txt");
            CHECK_EQ(url.str(), "file:///C:/folder/file.txt");
            CHECK_EQ(url.str(URL::StripScheme::yes), "C:/folder/file.txt");
        }
    }

    TEST_CASE("authentication")
    {
        URL url{};
        CHECK_EQ(url.authentication(), "");
        url.set_user("user");
        CHECK_EQ(url.authentication(), "user");
        url.set_password("password");
        CHECK_EQ(url.authentication(), "user:password");
    }

    TEST_CASE("authority")
    {
        URL url{};
        url.set_scheme("https")
            .set_host("mamba.org")
            .set_path("/folder/file.html")
            .set_query("param=value")
            .set_fragment("fragment");
        CHECK_EQ(url.authority(), "mamba.org");
        url.set_port("8000");
        CHECK_EQ(url.authority(), "mamba.org:8000");
        url.set_user("user");
        CHECK_EQ(url.authority(), "user@mamba.org:8000");
        url.set_password("password");
        CHECK_EQ(url.authority(), "user:password@mamba.org:8000");
    }

    TEST_CASE("Equality")
    {
        CHECK_EQ(URL(), URL());
        CHECK_EQ(URL::parse("https://169.254.0.0/page"), URL::parse("https://169.254.0.0/page"));
        CHECK_EQ(URL::parse("mamba.org"), URL::parse("mamba.org/"));
        CHECK_EQ(URL::parse("mAmba.oRg"), URL::parse("mamba.org/"));
        CHECK_EQ(URL::parse("localhost/page"), URL::parse("https://localhost/page"));

        CHECK_NE(URL::parse("mamba.org/page"), URL::parse("mamba.org/"));
        CHECK_NE(URL::parse("mamba.org"), URL::parse("mamba.org:9999"));
        CHECK_NE(URL::parse("user@mamba.org"), URL::parse("mamba.org"));
        CHECK_NE(URL::parse("mamba.org/page"), URL::parse("mamba.org/page?q=v"));
        CHECK_NE(URL::parse("mamba.org/page"), URL::parse("mamba.org/page#there"));
    }

    TEST_CASE("Append path")
    {
        auto url = URL();

        CHECK_EQ(url.path(), "/");
        CHECK_EQ((url / "").path(), "/");
        CHECK_EQ((url / "   ").path(), "/");
        CHECK_EQ((url / "/").path(), "/");
        CHECK_EQ((url / "page").path(), "/page");
        CHECK_EQ((url / "/page").path(), "/page");
        CHECK_EQ((url / " /page").path(), "/page");
        CHECK_EQ(url.path(), "/");  // unchanged

        url.append_path("folder");
        CHECK_EQ(url.path(), "/folder");
        CHECK_EQ((url / "").path(), "/folder");
        CHECK_EQ((url / "/").path(), "/folder/");
        CHECK_EQ((url / "page").path(), "/folder/page");
        CHECK_EQ((url / "/page").path(), "/folder/page");
    }
}
