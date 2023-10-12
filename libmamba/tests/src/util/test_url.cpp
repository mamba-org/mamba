// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>
#include <string_view>

#include <doctest/doctest.h>

#include "mamba/util/build.hpp"
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
            CHECK_EQ(url.user(), "");
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

        SUBCASE("Complete")
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
            CHECK_EQ(url.user(), "user");
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

        SUBCASE("File")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("/folder/file.txt");
            CHECK_EQ(url.scheme(), "file");
            CHECK_EQ(url.host(), "");
            CHECK_EQ(url.path(), "/folder/file.txt");
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

        SUBCASE("Case")
        {
            URL url{};
            url.set_scheme("FtP");
            url.set_host("sOme_Host.COM");
            CHECK_EQ(url.scheme(), "ftp");
            CHECK_EQ(url.host(), "some_host.com");
        }

        SUBCASE("Default scheme")
        {
            URL url{};
            CHECK(url.scheme_is_defaulted());
            CHECK_EQ(url.scheme(), "https");

            url.set_scheme("https");
            CHECK_FALSE(url.scheme_is_defaulted());
            CHECK_EQ(url.scheme(), "https");

            url.set_scheme("");
            CHECK(url.scheme_is_defaulted());
            url.set_scheme("https");

            url.set_scheme("ftp");
            CHECK_FALSE(url.scheme_is_defaulted());
            CHECK_EQ(url.scheme(), "ftp");

            CHECK_EQ(url.clear_scheme(), "ftp");
            CHECK(url.scheme_is_defaulted());
            url.set_scheme("https");
        }

        SUBCASE("Default host")
        {
            URL url{};
            CHECK(url.host_is_defaulted());
            CHECK_EQ(url.host(), "localhost");

            url.set_host("localhost");
            CHECK_FALSE(url.host_is_defaulted());
            CHECK_EQ(url.host(), "localhost");

            url.set_host("");
            CHECK(url.host_is_defaulted());
            url.set_host("localhost");

            url.set_host("test.org");
            CHECK_FALSE(url.host_is_defaulted());
            CHECK_EQ(url.host(), "test.org");

            CHECK_EQ(url.clear_host(), "test.org");
            CHECK(url.host_is_defaulted());
            url.set_host("localhost");
        }

        SUBCASE("Invalid")
        {
            URL url{};
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
            url.set_host("micro#mamba.org", URL::Encode::yes);
            CHECK_EQ(url.host(URL::Decode::no), "micro%23mamba.org");
            CHECK_EQ(url.host(URL::Decode::yes), "micro#mamba.org");
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

        SUBCASE("http://user@40email.com:test@localhost")
        {
            // Fails before "user@email.com" needs to be percent encoded, otherwise parsing is
            // ill defined.

            // Silencing [[nodiscard]] warning
            auto failure = [](std::string_view str) { [[maybe_unused]] auto url = URL::parse(str); };
            CHECK_THROWS_AS(failure("http://user@40email.com:test@localhost"), std::invalid_argument);
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
            if (on_win)
            {
                const URL url = URL::parse("file://C:/Users/wolfv/test/document.json");
                CHECK_EQ(url.scheme(), "file");
                CHECK_EQ(url.host(), "");
                CHECK_EQ(url.path(), "/C:/Users/wolfv/test/document.json");
                if (on_win)
                {
                    CHECK_EQ(url.pretty_path(), "C:/Users/wolfv/test/document.json");
                }
                else
                {
                    CHECK_EQ(url.pretty_path(), "/C:/Users/wolfv/test/document.json");
                }
                CHECK_EQ(url.user(), "");
                CHECK_EQ(url.password(), "");
                CHECK_EQ(url.port(), "");
                CHECK_EQ(url.query(), "");
                CHECK_EQ(url.fragment(), "");
            }
        }

        SUBCASE("file:///home/wolfv/test/document.json")
        {
            if (!on_win)
            {
                const URL url = URL::parse("file:///home/wolfv/test/document.json");
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

        SUBCASE("https://mamba.org/page#the-fragment")
        {
            const URL url = URL::parse("https://mamba.org/page#the-fragment");
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

        SUBCASE("without credentials")
        {
            CHECK_EQ(url.str(URL::Credentials::Show), "https://localhost/");
            CHECK_EQ(url.str(URL::Credentials::Hide), "https://localhost/");
            CHECK_EQ(url.str(URL::Credentials::Remove), "https://localhost/");
        }

        SUBCASE("with some credentials")
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
        SUBCASE("scheme option")
        {
            URL url = {};
            url.set_host("mamba.org");

            SUBCASE("defaut scheme")
            {
                CHECK_EQ(url.pretty_str(URL::StripScheme::no), "https://mamba.org/");
                CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "mamba.org/");
            }

            SUBCASE("ftp scheme")
            {
                url.set_scheme("ftp");
                CHECK_EQ(url.pretty_str(URL::StripScheme::no), "ftp://mamba.org/");
                CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "mamba.org/");
            }
        }

        SUBCASE("rstrip option")
        {
            URL url = {};
            url.set_host("mamba.org");
            CHECK_EQ(url.pretty_str(URL::StripScheme::no, 0), "https://mamba.org/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::no, '/'), "https://mamba.org");
            url.set_path("/page/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::no, ':'), "https://mamba.org/page/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::no, '/'), "https://mamba.org/page");
        }

        SUBCASE("Credential option")
        {
            URL url = {};

            SUBCASE("without credentials")
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

            SUBCASE("with some credentials")
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
        SUBCASE("https://user:password@mamba.org:8080/folder/file.html?param=value#fragment")
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
                "https://user:password@mamba.org:8080/folder/file.html?param=value#fragment"
            );
            CHECK_EQ(
                url.pretty_str(),
                "https://user:password@mamba.org:8080/folder/file.html?param=value#fragment"
            );
        }

        SUBCASE("user@mamba.org")
        {
            URL url{};
            url.set_host("mamba.org");
            url.set_user("user");
            CHECK_EQ(url.pretty_str(), "https://user@mamba.org/");
            CHECK_EQ(url.str(), "https://user@mamba.org/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "user@mamba.org/");
        }

        SUBCASE("https://mamba.org")
        {
            URL url{};
            url.set_scheme("https");
            url.set_host("mamba.org");
            CHECK_EQ(url.str(), "https://mamba.org/");
            CHECK_EQ(url.pretty_str(), "https://mamba.org/");
            CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "mamba.org/");
        }

        SUBCASE("file:////folder/file.txt")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("//folder/file.txt");
            CHECK_EQ(url.str(), "file:////folder/file.txt");
            CHECK_EQ(url.pretty_str(), "file:////folder/file.txt");
            CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "//folder/file.txt");
        }

        SUBCASE("file:///folder/file.txt")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("/folder/file.txt");
            CHECK_EQ(url.str(), "file:///folder/file.txt");
            CHECK_EQ(url.pretty_str(), "file:///folder/file.txt");
            CHECK_EQ(url.pretty_str(URL::StripScheme::yes), "/folder/file.txt");
        }

        SUBCASE("file:///C:/folder&/file.txt")
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

        SUBCASE("https://user@email.com:pw%rd@mamba.org/some /path$/")
        {
            URL url{};
            url.set_scheme("https");
            url.set_host("mamba.org");
            url.set_user("user@email.com");
            url.set_password("pw%rd");
            url.set_path("/some /path$/");
            CHECK_EQ(url.str(), "https://user%40email.com:pw%25rd@mamba.org/some%20/path%24/");
            CHECK_EQ(url.pretty_str(), "https://user@email.com:pw%rd@mamba.org/some /path$/");
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
        CHECK_EQ(url.authority(), "user%40email.com@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Show), "user%40email.com@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Hide), "user%40email.com:*****@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Remove), "mamba.org:8000");

        url.set_password("pass");
        CHECK_EQ(url.authority(), "user%40email.com:pass@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Show), "user%40email.com:pass@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Hide), "user%40email.com:*****@mamba.org:8000");
        CHECK_EQ(url.authority(URL::Credentials::Remove), "mamba.org:8000");
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

        SUBCASE("Add components")
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

        SUBCASE("Absolute paths")
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
}
