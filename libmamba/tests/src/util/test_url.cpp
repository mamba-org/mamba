// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>
#include <string_view>

#include <catch2/catch_all.hpp>

#include "mamba/util/build.hpp"
#include "mamba/util/url.hpp"

using namespace mamba::util;

namespace
{
    TEST_CASE("URL builder")
    {
        SECTION("Empty")
        {
            URL url{};
            CHECK_EQ(url.scheme(), URL::https);
            REQUIRE_FALSE(url.has_user());
            CHECK_EQ(url.user(), "");
            REQUIRE_FALSE(url.has_password());
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.host(), URL::localhost);
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.pretty_path(), "/");
            CHECK_EQ(url.query(), "");

            CHECK_EQ(url.clear_user(), "");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.clear_password(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.clear_port(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.clear_host(), URL::localhost);
            CHECK_EQ(url.host(), URL::localhost);
            CHECK_EQ(url.clear_path(), "/");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.clear_query(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.clear_fragment(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SECTION("Complete")
        {
            URL url{};
            url.set_scheme("https");
            url.set_host("mamba.org");
            url.set_user("user");
            url.set_password("pass:word");
            url.set_port("8080");
            url.set_path("/folder/file.html");
            url.set_query("param=value");
            url.set_fragment("fragment");

            CHECK_EQ(url.scheme(), "https");
            CHECK_EQ(url.host(), "mamba.org");
            REQUIRE(url.has_user());
            CHECK_EQ(url.user(), "user");
            REQUIRE(url.has_password());
            CHECK_EQ(url.password(), "pass:word");
            CHECK_EQ(url.port(), "8080");
            CHECK_EQ(url.path(), "/folder/file.html");
            CHECK_EQ(url.pretty_path(), "/folder/file.html");
            CHECK_EQ(url.query(), "param=value");
            CHECK_EQ(url.fragment(), "fragment");

            CHECK_EQ(url.clear_user(), "user");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.clear_password(), "pass%3Aword");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.clear_port(), "8080");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.clear_host(), "mamba.org");
            CHECK_EQ(url.host(), URL::localhost);
            CHECK_EQ(url.clear_path(), "/folder/file.html");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.clear_query(), "param=value");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.clear_fragment(), "fragment");
            CHECK_EQ(url.fragment(), "");
        }

        SECTION("File")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("/folder/file.txt");
            CHECK_EQ(url.scheme(), "file");
            CHECK_EQ(url.host(), "");
            CHECK_EQ(url.path(), "/folder/file.txt");
        }

        SECTION("Path")
        {
            URL url{};
            url.set_path("path/");
            CHECK_EQ(url.path(), "/path/");
            CHECK_EQ(url.pretty_path(), "/path/");
        }

        SECTION("Windows path")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("C:/folder/file.txt");
            CHECK_EQ(url.path(), "/C:/folder/file.txt");
            if (on_win)
            {
                CHECK_EQ(url.path(URL::Decode::no), "/C:/folder/file.txt");
                CHECK_EQ(url.pretty_path(), "C:/folder/file.txt");
            }
            else
            {
                CHECK_EQ(url.path(URL::Decode::no), "/C%3A/folder/file.txt");
                CHECK_EQ(url.pretty_path(), "/C:/folder/file.txt");
            }
        }

        SECTION("Case")
        {
            URL url{};
            url.set_scheme("FtP");
            url.set_host("sOme_Host.COM");
            CHECK_EQ(url.scheme(), "ftp");
            CHECK_EQ(url.host(), "some_host.com");
        }

        SECTION("Default scheme")
        {
            URL url{};
            REQUIRE(url.scheme_is_defaulted());
            CHECK_EQ(url.scheme(), "https");

            url.set_scheme("https");
            REQUIRE_FALSE(url.scheme_is_defaulted());
            CHECK_EQ(url.scheme(), "https");

            url.set_scheme("");
            REQUIRE(url.scheme_is_defaulted());
            url.set_scheme("https");

            url.set_scheme("ftp");
            REQUIRE_FALSE(url.scheme_is_defaulted());
            CHECK_EQ(url.scheme(), "ftp");

            CHECK_EQ(url.clear_scheme(), "ftp");
            REQUIRE(url.scheme_is_defaulted());
            url.set_scheme("https");
        }

        SECTION("Default host")
        {
            URL url{};
            REQUIRE(url.host_is_defaulted());
            CHECK_EQ(url.host(), "localhost");

            url.set_host("localhost");
            REQUIRE_FALSE(url.host_is_defaulted());
            CHECK_EQ(url.host(), "localhost");

            url.set_host("");
            REQUIRE(url.host_is_defaulted());
            url.set_host("localhost");

            url.set_host("test.org");
            REQUIRE_FALSE(url.host_is_defaulted());
            CHECK_EQ(url.host(), "test.org");

            CHECK_EQ(url.clear_host(), "test.org");
            REQUIRE(url.host_is_defaulted());
            url.set_host("localhost");
        }

        SECTION("Invalid")
        {
            URL url{};
            CHECK_THROWS_AS(url.set_port("not-a-number"), std::invalid_argument);
        }

        SECTION("Encoding")
        {
            URL url{};
            url.set_user("micro@mamba.pm", URL::Encode::yes);
            CHECK_EQ(url.user(URL::Decode::no), "micro%40mamba.pm");
            CHECK_EQ(url.user(URL::Decode::yes), "micro@mamba.pm");
            url.set_password(R"(#!$&'"ab23)", URL::Encode::yes);
            CHECK_EQ(url.password(URL::Decode::no), "%23%21%24%26%27%22ab23");
            CHECK_EQ(url.password(URL::Decode::yes), R"(#!$&'"ab23)");
            url.set_host("micro#mamba.org", URL::Encode::yes);
            CHECK_EQ(url.host(URL::Decode::no), "micro%23mamba.org");
            CHECK_EQ(url.host(URL::Decode::yes), "micro#mamba.org");
        }
    }

    TEST_CASE("parse")
    {
        SECTION("Empty")
        {
            REQUIRE_FALSE(URL::parse("").has_value());
        }

        SECTION("mamba.org")
        {
            const URL url = URL::parse("mamba.org").value();
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

        SECTION("http://mamba.org")
        {
            const URL url = URL::parse("http://mamba.org").value();
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

        SECTION("s3://userx123:üúßsajd@mamba.org")
        {
            const URL url = URL::parse("s3://userx123:üúßsajd@mamba.org").value();
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

        SECTION("http://user%40email.com:test@localhost:8000")
        {
            const URL url = URL::parse("http://user%40email.com:test@localhost:8000").value();
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

        SECTION("http://user@40email.com:test@localhost")
        {
            // Fails before "user@email.com" needs to be percent encoded, otherwise parsing is
            // ill defined.
            REQUIRE_FALSE(URL::parse("http://user@40email.com:test@localhost").has_value());
        }

        SECTION("http://:pass@localhost:8000")
        {
            const URL url = URL::parse("http://:pass@localhost:8000").value();
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

        SECTION("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333")
        {
            // Not a valid IETF RFC 3986+ URL, but Curl parses it anyways.
            // Undefined behavior, no assumptions are made
            const URL url = URL::parse("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333")
                                .value();
            CHECK_NE(url.host(URL::Decode::no), "mamba%f0%9f%86%92%f0%9f%94%ac.org");
        }

        SECTION("file://C:/Users/wolfv/test/document.json")
        {
            if (on_win)
            {
                const URL url = URL::parse("file://C:/Users/wolfv/test/document.json").value();
                CHECK_EQ(url.scheme(), "file");
                CHECK_EQ(url.host(), "");
                CHECK_EQ(url.path(), "/C:/Users/wolfv/test/document.json");
                CHECK_EQ(url.path(URL::Decode::no), "/C:/Users/wolfv/test/document.json");
                CHECK_EQ(url.pretty_path(), "C:/Users/wolfv/test/document.json");
                CHECK_EQ(url.user(), "");
                CHECK_EQ(url.password(), "");
                CHECK_EQ(url.port(), "");
                CHECK_EQ(url.query(), "");
                CHECK_EQ(url.fragment(), "");
            }
        }

        SECTION("file:///home/wolfv/test/document.json")
        {
            const URL url = URL::parse("file:///home/wolfv/test/document.json").value();
            CHECK_EQ(url.scheme(), "file");
            CHECK_EQ(url.host(), "");
            CHECK_EQ(url.path(), "/home/wolfv/test/document.json");
            CHECK_EQ(url.pretty_path(), "/home/wolfv/test/document.json");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SECTION("file:///home/great:doc.json")
        {
            // Not a valid IETF RFC 3986+ URL, but Curl parses it anyways.
            // Undefined behavior, no assumptions are made
            const URL url = URL::parse("file:///home/great:doc.json").value();
            CHECK_NE(url.path(URL::Decode::no), "/home/great%3Adoc.json");
        }

        SECTION("file:///home/great%3Adoc.json")
        {
            const URL url = URL::parse("file:///home/great%3Adoc.json").value();
            CHECK_EQ(url.scheme(), "file");
            CHECK_EQ(url.host(), "");
            CHECK_EQ(url.path(), "/home/great:doc.json");
            CHECK_EQ(url.path(URL::Decode::no), "/home/great%3Adoc.json");
            CHECK_EQ(url.pretty_path(), "/home/great:doc.json");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "");
        }

        SECTION("https://169.254.0.0/page")
        {
            const URL url = URL::parse("https://169.254.0.0/page").value();
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

        SECTION("ftp://user:pass@[2001:db8:85a3:8d3:1319:0:370:7348]:9999/page")
        {
            const URL url = URL::parse("ftp://user:pass@[2001:db8:85a3:8d3:1319:0:370:7348]:9999/page")
                                .value();
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

        SECTION("https://mamba.org/page#the-fragment")
        {
            const URL url = URL::parse("https://mamba.org/page#the-fragment").value();
            CHECK_EQ(url.scheme(), "https");
            CHECK_EQ(url.host(), "mamba.org");
            CHECK_EQ(url.path(), "/page");
            CHECK_EQ(url.pretty_path(), "/page");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.query(), "");
            CHECK_EQ(url.fragment(), "the-fragment");
        }
    }

    TEST_CASE("str options")
    {
        URL url = {};

        SECTION("without credentials")
        {
            CHECK_EQ(url.str(URL::Credentials::Show), "https://localhost/");
            CHECK_EQ(url.str(URL::Credentials::Hide), "https://localhost/");
            CHECK_EQ(url.str(URL::Credentials::Remove), "https://localhost/");
        }

        SECTION("with some credentials")
        {
            url.set_user("user@mamba.org");
            url.set_password("pass");

            CHECK_EQ(url.str(URL::Credentials::Show), "https://user%40mamba.org:pass@localhost/");
            CHECK_EQ(url.str(URL::Credentials::Hide), "https://user%40mamba.org:*****@localhost/");
            CHECK_EQ(url.str(URL::Credentials::Remove), "https://localhost/");
        }
    }

    TEST_CASE("pretty_str options")
    {
        SECTION("scheme option")
        {
            URL url = {};
            url.set_host("mamba.org");

            SECTION("default scheme")
            {
                CHECK_EQ(url.pretty_str(URL::StripScheme::no), "https://mamba.org/");
                CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "mamba.org/");
            }

            SECTION("ftp scheme")
            {
                url.set_scheme("ftp");
                CHECK_EQ(url.pretty_str(URL::StripScheme::no), "ftp://mamba.org/");
                CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "mamba.org/");
            }
        }

        SECTION("rstrip option")
        {
            URL url = {};
            url.set_host("mamba.org");
            CHECK_EQ(url.pretty_str(URL::StripScheme::no, 0), "https://mamba.org/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::no, '/'), "https://mamba.org");
            url.set_path("/page/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::no, ':'), "https://mamba.org/page/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::no, '/'), "https://mamba.org/page");
        }

        SECTION("Credential option")
        {
            URL url = {};

            SECTION("without credentials")
            {
                CHECK_EQ(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Show),
                    "https://localhost/"
                );
                CHECK_EQ(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Hide),
                    "https://localhost/"
                );
                CHECK_EQ(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Remove),
                    "https://localhost/"
                );
            }

            SECTION("with some credentials")
            {
                url.set_user("user");
                url.set_password("pass");

                CHECK_EQ(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Show),
                    "https://user:pass@localhost/"
                );
                CHECK_EQ(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Hide),
                    "https://user:*****@localhost/"
                );
                CHECK_EQ(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Remove),
                    "https://localhost/"
                );
            }
        }
    }

    TEST_CASE("str and pretty_str")
    {
        SECTION("https://user:password@mamba.org:8080/folder/file.html?param=value#fragment")
        {
            URL url{};
            url.set_scheme("https");
            url.set_host("mamba.org");
            url.set_user("user");
            url.set_password("password");
            url.set_port("8080");
            url.set_path("/folder/file.html");
            url.set_query("param=value");
            url.set_fragment("fragment");

            CHECK_EQ(
                url.str(),
                "https://user:*****@mamba.org:8080/folder/file.html?param=value#fragment"
            );
            CHECK_EQ(
                url.str(URL::Credentials::Show),
                "https://user:password@mamba.org:8080/folder/file.html?param=value#fragment"
            );
            CHECK_EQ(
                url.str(URL::Credentials::Hide),
                "https://user:*****@mamba.org:8080/folder/file.html?param=value#fragment"
            );
            CHECK_EQ(
                url.str(URL::Credentials::Remove),
                "https://mamba.org:8080/folder/file.html?param=value#fragment"
            );
            CHECK_EQ(
                url.pretty_str(),
                "https://user:*****@mamba.org:8080/folder/file.html?param=value#fragment"
            );
        }

        SECTION("user@mamba.org")
        {
            URL url{};
            url.set_host("mamba.org");
            url.set_user("user");
            CHECK_EQ(url.str(URL::Credentials::Show), "https://user@mamba.org/");
            CHECK_EQ(url.str(URL::Credentials::Hide), "https://user:*****@mamba.org/");
            CHECK_EQ(url.pretty_str(), "https://user:*****@mamba.org/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "user:*****@mamba.org/");
            CHECK_EQ(
                url.pretty_str(URL::StripScheme::yes, '\0', URL::Credentials::Hide),
                "user:*****@mamba.org/"
            );
            CHECK_EQ(
                url.pretty_str(URL::StripScheme::yes, '\0', URL::Credentials::Show),
                "user@mamba.org/"
            );
            CHECK_EQ(url.pretty_str(URL::StripScheme::yes, '\0', URL::Credentials::Remove), "mamba.org/");
        }

        SECTION("https://mamba.org")
        {
            URL url{};
            url.set_scheme("https");
            url.set_host("mamba.org");
            CHECK_EQ(url.str(), "https://mamba.org/");
            CHECK_EQ(url.pretty_str(), "https://mamba.org/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "mamba.org/");
        }

        SECTION("file:////folder/file.txt")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("//folder/file.txt");
            CHECK_EQ(url.str(), "file:////folder/file.txt");
            CHECK_EQ(url.pretty_str(), "file:////folder/file.txt");
            CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "//folder/file.txt");
        }

        SECTION("file:///folder/file.txt")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("/folder/file.txt");
            CHECK_EQ(url.str(), "file:///folder/file.txt");
            CHECK_EQ(url.pretty_str(), "file:///folder/file.txt");
            CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "/folder/file.txt");
        }

        SECTION("file:///C:/folder&/file.txt")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("C:/folder&/file.txt");
            if (on_win)
            {
                CHECK_EQ(url.str(), "file:///C:/folder%26/file.txt");
            }
            else
            {
                CHECK_EQ(url.str(), "file:///C%3A/folder%26/file.txt");
            }
            CHECK_EQ(url.pretty_str(), "file:///C:/folder&/file.txt");
            if (on_win)
            {
                CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "C:/folder&/file.txt");
            }
            else
            {
                CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "/C:/folder&/file.txt");
            }
        }

        SECTION("https://user@email.com:pw%rd@mamba.org/some /path$/")
        {
            URL url{};
            url.set_scheme("https");
            url.set_host("mamba.org");
            url.set_user("user@email.com");
            url.set_password("pw%rd");
            url.set_path("/some /path$/");
            CHECK_EQ(
                url.str(URL::Credentials::Show),
                "https://user%40email.com:pw%25rd@mamba.org/some%20/path%24/"
            );
            CHECK_EQ(
                url.pretty_str(URL::StripScheme::no, '/', URL::Credentials::Show),
                "https://user@email.com:pw%rd@mamba.org/some /path$"
            );
        }
    }

    TEST_CASE("authentication")
    {
        URL url{};
        CHECK_EQ(url.authentication(), "");
        url.set_user("user@email.com");
        CHECK_EQ(url.authentication(), "user%40email.com");
        url.set_password("password");
        CHECK_EQ(url.authentication(), "user%40email.com:password");
    }

    TEST_CASE("authority")
    {
        URL url{};
        url.set_scheme("https");
        url.set_host("mamba.org");
        url.set_path("/folder/file.html");
        url.set_query("param=value");
        url.set_fragment("fragment");
        CHECK_EQ(url.authority(), "mamba.org");
        CHECK_EQ(url.authority(URL::Credentials::Show), "mamba.org");
        CHECK_EQ(url.authority(URL::Credentials::Hide), "mamba.org");
        CHECK_EQ(url.authority(URL::Credentials::Remove), "mamba.org");

        url.set_port("8000");
        CHECK_EQ(url.authority(), "mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Show), "mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Hide), "mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Remove), "mamba.org:8000");

        url.set_user("user@email.com");
        CHECK_EQ(url.authority(), "user%40email.com:*****@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Show), "user%40email.com@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Hide), "user%40email.com:*****@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Remove), "mamba.org:8000");

        url.set_password("pass");
        CHECK_EQ(url.authority(), "user%40email.com:*****@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Show), "user%40email.com:pass@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Hide), "user%40email.com:*****@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Remove), "mamba.org:8000");
    }

    TEST_CASE("Equality")
    {
        REQUIRE(URL() == URL();
        CHECK_EQ(
            URL::parse("https://169.254.0.0/page").value(),
            URL::parse("https://169.254.0.0/page").value()
        );
        CHECK_EQ(URL::parse("mamba.org").value(), URL::parse("mamba.org/").value());
        CHECK_EQ(URL::parse("mAmba.oRg").value(), URL::parse("mamba.org/").value());
        CHECK_EQ(URL::parse("localhost/page").value(), URL::parse("https://localhost/page").value());

        CHECK_NE(URL::parse("mamba.org/page").value(), URL::parse("mamba.org/").value());
        CHECK_NE(URL::parse("mamba.org").value(), URL::parse("mamba.org:9999").value());
        CHECK_NE(URL::parse("user@mamba.org").value(), URL::parse("mamba.org").value());
        CHECK_NE(URL::parse("mamba.org/page").value(), URL::parse("mamba.org/page?q=v").value());
        CHECK_NE(URL::parse("mamba.org/page").value(), URL::parse("mamba.org/page#there").value());
    }

    TEST_CASE("Append path")
    {
        auto url = URL();

        SECTION("Add components")
        {
            CHECK_EQ(url.path(), "/");
            CHECK_EQ((url / "").path(), "/");
            CHECK_EQ((url / "   ").path(), "/   ");
            CHECK_EQ((url / "/").path(), "/");
            CHECK_EQ((url / "page").path(), "/page");
            CHECK_EQ((url / "/page").path(), "/page");
            CHECK_EQ((url / " /page").path(), "/ /page");
            CHECK_EQ(url.path(), "/");  // unchanged

            url.append_path("folder");
            CHECK_EQ(url.path(), "/folder");
            CHECK_EQ((url / "").path(), "/folder");
            CHECK_EQ((url / "/").path(), "/folder/");
            CHECK_EQ((url / "page").path(), "/folder/page");
            CHECK_EQ((url / "/page").path(), "/folder/page");
        }

        SECTION("Absolute paths")
        {
            url.set_scheme("file");
            url.append_path("C:/folder/file.txt");
            if (on_win)
            {
                CHECK_EQ(url.str(), "file:///C:/folder/file.txt");
            }
            else
            {
                CHECK_EQ(url.str(), "file:///C%3A/folder/file.txt");
            }
        }
    }

    TEST_CASE("Comparison")
    {
        URL url{};
        url.set_scheme("https");
        url.set_user("user");
        url.set_password("password");
        url.set_host("mamba.org");
        url.set_port("33");
        url.set_path("/folder/file.html");
        auto other = url;

        CHECK_EQ(url, other);

        SECTION("Different scheme")
        {
            other.set_scheme("ftp");
            CHECK_NE(url, other);
        }

        SECTION("Different hosts")
        {
            other.clear_host();
            CHECK_NE(url, other);
        }

        SECTION("Different users")
        {
            other.clear_user();
            CHECK_NE(url, other);
        }

        SECTION("Different passwords")
        {
            other.clear_password();
            CHECK_NE(url, other);
        }

        SECTION("Different ports")
        {
            other.clear_port();
            CHECK_NE(url, other);
        }

        SECTION("Different path")
        {
            other.clear_path();
            CHECK_NE(url, other);
        }
    }
}
