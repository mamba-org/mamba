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
            REQUIRE(url.scheme() == URL::https);
            REQUIRE_FALSE(url.has_user());
            REQUIRE(url.user() == "");
            REQUIRE_FALSE(url.has_password());
            REQUIRE(url.password() == "");
            REQUIRE(url.port() == "");
            REQUIRE(url.host() == URL::localhost);
            REQUIRE(url.path() == "/");
            REQUIRE(url.pretty_path() == "/");
            REQUIRE(url.query() == "");

            REQUIRE(url.clear_user() == "");
            REQUIRE(url.user() == "");
            REQUIRE(url.clear_password() == "");
            REQUIRE(url.password() == "");
            REQUIRE(url.clear_port() == "");
            REQUIRE(url.port() == "");
            REQUIRE(url.clear_host() == URL::localhost);
            REQUIRE(url.host() == URL::localhost);
            REQUIRE(url.clear_path() == "/");
            REQUIRE(url.path() == "/");
            REQUIRE(url.clear_query() == "");
            REQUIRE(url.query() == "");
            REQUIRE(url.clear_fragment() == "");
            REQUIRE(url.fragment() == "");
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

            REQUIRE(url.scheme() == "https");
            REQUIRE(url.host() == "mamba.org");
            REQUIRE(url.has_user());
            REQUIRE(url.user() == "user");
            REQUIRE(url.has_password());
            REQUIRE(url.password() == "pass:word");
            REQUIRE(url.port() == "8080");
            REQUIRE(url.path() == "/folder/file.html");
            REQUIRE(url.pretty_path() == "/folder/file.html");
            REQUIRE(url.query() == "param=value");
            REQUIRE(url.fragment() == "fragment");

            REQUIRE(url.clear_user() == "user");
            REQUIRE(url.user() == "");
            REQUIRE(url.clear_password() == "pass%3Aword");
            REQUIRE(url.password() == "");
            REQUIRE(url.clear_port() == "8080");
            REQUIRE(url.port() == "");
            REQUIRE(url.clear_host() == "mamba.org");
            REQUIRE(url.host() == URL::localhost);
            REQUIRE(url.clear_path() == "/folder/file.html");
            REQUIRE(url.path() == "/");
            REQUIRE(url.clear_query() == "param=value");
            REQUIRE(url.query() == "");
            REQUIRE(url.clear_fragment() == "fragment");
            REQUIRE(url.fragment() == "");
        }

        SECTION("File")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("/folder/file.txt");
            REQUIRE(url.scheme() == "file");
            REQUIRE(url.host() == "");
            REQUIRE(url.path() == "/folder/file.txt");
        }

        SECTION("Path")
        {
            URL url{};
            url.set_path("path/");
            REQUIRE(url.path() == "/path/");
            REQUIRE(url.pretty_path() == "/path/");
        }

        SECTION("Windows path")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("C:/folder/file.txt");
            REQUIRE(url.path() == "/C:/folder/file.txt");
            if (on_win)
            {
                REQUIRE(url.path(URL::Decode::no) == "/C:/folder/file.txt");
                REQUIRE(url.pretty_path() == "C:/folder/file.txt");
            }
            else
            {
                REQUIRE(url.path(URL::Decode::no) == "/C%3A/folder/file.txt");
                REQUIRE(url.pretty_path() == "/C:/folder/file.txt");
            }
        }

        SECTION("Case")
        {
            URL url{};
            url.set_scheme("FtP");
            url.set_host("sOme_Host.COM");
            REQUIRE(url.scheme() == "ftp");
            REQUIRE(url.host() == "some_host.com");
        }

        SECTION("Default scheme")
        {
            URL url{};
            REQUIRE(url.scheme_is_defaulted());
            REQUIRE(url.scheme() == "https");

            url.set_scheme("https");
            REQUIRE_FALSE(url.scheme_is_defaulted());
            REQUIRE(url.scheme() == "https");

            url.set_scheme("");
            REQUIRE(url.scheme_is_defaulted());
            url.set_scheme("https");

            url.set_scheme("ftp");
            REQUIRE_FALSE(url.scheme_is_defaulted());
            REQUIRE(url.scheme() == "ftp");

            REQUIRE(url.clear_scheme() == "ftp");
            REQUIRE(url.scheme_is_defaulted());
            url.set_scheme("https");
        }

        SECTION("Default host")
        {
            URL url{};
            REQUIRE(url.host_is_defaulted());
            REQUIRE(url.host() == "localhost");

            url.set_host("localhost");
            REQUIRE_FALSE(url.host_is_defaulted());
            REQUIRE(url.host() == "localhost");

            url.set_host("");
            REQUIRE(url.host_is_defaulted());
            url.set_host("localhost");

            url.set_host("test.org");
            REQUIRE_FALSE(url.host_is_defaulted());
            REQUIRE(url.host() == "test.org");

            REQUIRE(url.clear_host() == "test.org");
            REQUIRE(url.host_is_defaulted());
            url.set_host("localhost");
        }

        SECTION("Invalid")
        {
            URL url{};
            REQUIRE_THROWS_AS(url.set_port("not-a-number"), std::invalid_argument);
        }

        SECTION("Encoding")
        {
            URL url{};
            url.set_user("micro@mamba.pm", URL::Encode::yes);
            REQUIRE(url.user(URL::Decode::no) == "micro%40mamba.pm");
            REQUIRE(url.user(URL::Decode::yes) == "micro@mamba.pm");
            url.set_password(R"(#!$&'"ab23)", URL::Encode::yes);
            REQUIRE(url.password(URL::Decode::no) == "%23%21%24%26%27%22ab23");
            REQUIRE(url.password(URL::Decode::yes) == R"(#!$&'"ab23)");
            url.set_host("micro#mamba.org", URL::Encode::yes);
            REQUIRE(url.host(URL::Decode::no) == "micro%23mamba.org");
            REQUIRE(url.host(URL::Decode::yes) == "micro#mamba.org");
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
            REQUIRE(url.scheme() == URL::https);
            REQUIRE(url.host() == "mamba.org");
            REQUIRE(url.path() == "/");
            REQUIRE(url.pretty_path() == "/");
            REQUIRE(url.user() == "");
            REQUIRE(url.password() == "");
            REQUIRE(url.port() == "");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "");
        }

        SECTION("http://mamba.org")
        {
            const URL url = URL::parse("http://mamba.org").value();
            REQUIRE(url.scheme() == "http");
            REQUIRE(url.host() == "mamba.org");
            REQUIRE(url.path() == "/");
            REQUIRE(url.pretty_path() == "/");
            REQUIRE(url.user() == "");
            REQUIRE(url.password() == "");
            REQUIRE(url.port() == "");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "");
        }

        SECTION("s3://userx123:üúßsajd@mamba.org")
        {
            const URL url = URL::parse("s3://userx123:üúßsajd@mamba.org").value();
            REQUIRE(url.scheme() == "s3");
            REQUIRE(url.host() == "mamba.org");
            REQUIRE(url.path() == "/");
            REQUIRE(url.pretty_path() == "/");
            REQUIRE(url.user() == "userx123");
            REQUIRE(url.password() == "üúßsajd");
            REQUIRE(url.port() == "");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "");
        }

        SECTION("http://user%40email.com:test@localhost:8000")
        {
            const URL url = URL::parse("http://user%40email.com:test@localhost:8000").value();
            REQUIRE(url.scheme() == "http");
            REQUIRE(url.host() == "localhost");
            REQUIRE(url.path() == "/");
            REQUIRE(url.pretty_path() == "/");
            REQUIRE(url.user() == "user@email.com");
            REQUIRE(url.password() == "test");
            REQUIRE(url.port() == "8000");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "");
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
            REQUIRE(url.scheme() == "http");
            REQUIRE(url.host() == "localhost");
            REQUIRE(url.path() == "/");
            REQUIRE(url.pretty_path() == "/");
            REQUIRE(url.user() == "");
            REQUIRE(url.password() == "pass");
            REQUIRE(url.port() == "8000");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "");
        }

        SECTION("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333")
        {
            // Not a valid IETF RFC 3986+ URL, but Curl parses it anyways.
            // Undefined behavior, no assumptions are made
            const URL url = URL::parse("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333")
                                .value();
            REQUIRE(url.host(URL::Decode::no) != "mamba%f0%9f%86%92%f0%9f%94%ac.org");
        }

        SECTION("file://C:/Users/wolfv/test/document.json")
        {
            if (on_win)
            {
                const URL url = URL::parse("file://C:/Users/wolfv/test/document.json").value();
                REQUIRE(url.scheme() == "file");
                REQUIRE(url.host() == "");
                REQUIRE(url.path() == "/C:/Users/wolfv/test/document.json");
                REQUIRE(url.path(URL::Decode::no) == "/C:/Users/wolfv/test/document.json");
                REQUIRE(url.pretty_path() == "C:/Users/wolfv/test/document.json");
                REQUIRE(url.user() == "");
                REQUIRE(url.password() == "");
                REQUIRE(url.port() == "");
                REQUIRE(url.query() == "");
                REQUIRE(url.fragment() == "");
            }
        }

        SECTION("file:///home/wolfv/test/document.json")
        {
            const URL url = URL::parse("file:///home/wolfv/test/document.json").value();
            REQUIRE(url.scheme() == "file");
            REQUIRE(url.host() == "");
            REQUIRE(url.path() == "/home/wolfv/test/document.json");
            REQUIRE(url.pretty_path() == "/home/wolfv/test/document.json");
            REQUIRE(url.user() == "");
            REQUIRE(url.password() == "");
            REQUIRE(url.port() == "");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "");
        }

        SECTION("file:///home/great:doc.json")
        {
            // Not a valid IETF RFC 3986+ URL, but Curl parses it anyways.
            // Undefined behavior, no assumptions are made
            const URL url = URL::parse("file:///home/great:doc.json").value();
            REQUIRE(url.path(URL::Decode::no) != "/home/great%3Adoc.json");
        }

        SECTION("file:///home/great%3Adoc.json")
        {
            const URL url = URL::parse("file:///home/great%3Adoc.json").value();
            REQUIRE(url.scheme() == "file");
            REQUIRE(url.host() == "");
            REQUIRE(url.path() == "/home/great:doc.json");
            REQUIRE(url.path(URL::Decode::no) == "/home/great%3Adoc.json");
            REQUIRE(url.pretty_path() == "/home/great:doc.json");
            REQUIRE(url.user() == "");
            REQUIRE(url.password() == "");
            REQUIRE(url.port() == "");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "");
        }

        SECTION("https://169.254.0.0/page")
        {
            const URL url = URL::parse("https://169.254.0.0/page").value();
            REQUIRE(url.scheme() == "https");
            REQUIRE(url.host() == "169.254.0.0");
            REQUIRE(url.path() == "/page");
            REQUIRE(url.pretty_path() == "/page");
            REQUIRE(url.user() == "");
            REQUIRE(url.password() == "");
            REQUIRE(url.port() == "");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "");
        }

        SECTION("ftp://user:pass@[2001:db8:85a3:8d3:1319:0:370:7348]:9999/page")
        {
            const URL url = URL::parse("ftp://user:pass@[2001:db8:85a3:8d3:1319:0:370:7348]:9999/page")
                                .value();
            REQUIRE(url.scheme() == "ftp");
            REQUIRE(url.host() == "[2001:db8:85a3:8d3:1319:0:370:7348]");
            REQUIRE(url.path() == "/page");
            REQUIRE(url.pretty_path() == "/page");
            REQUIRE(url.user() == "user");
            REQUIRE(url.password() == "pass");
            REQUIRE(url.port() == "9999");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "");
        }

        SECTION("https://mamba.org/page#the-fragment")
        {
            const URL url = URL::parse("https://mamba.org/page#the-fragment").value();
            REQUIRE(url.scheme() == "https");
            REQUIRE(url.host() == "mamba.org");
            REQUIRE(url.path() == "/page");
            REQUIRE(url.pretty_path() == "/page");
            REQUIRE(url.user() == "");
            REQUIRE(url.password() == "");
            REQUIRE(url.port() == "");
            REQUIRE(url.query() == "");
            REQUIRE(url.fragment() == "the-fragment");
        }
    }

    TEST_CASE("str options")
    {
        URL url = {};

        SECTION("without credentials")
        {
            REQUIRE(url.str(URL::Credentials::Show) == "https://localhost/");
            REQUIRE(url.str(URL::Credentials::Hide) == "https://localhost/");
            REQUIRE(url.str(URL::Credentials::Remove) == "https://localhost/");
        }

        SECTION("with some credentials")
        {
            url.set_user("user@mamba.org");
            url.set_password("pass");

            REQUIRE(url.str(URL::Credentials::Show) == "https://user%40mamba.org:pass@localhost/");
            REQUIRE(url.str(URL::Credentials::Hide) == "https://user%40mamba.org:*****@localhost/");
            REQUIRE(url.str(URL::Credentials::Remove) == "https://localhost/");
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
                REQUIRE(url.pretty_str(URL::StripScheme::no) == "https://mamba.org/");
                REQUIRE(url.pretty_str(URL::StripScheme::yes) == "mamba.org/");
            }

            SECTION("ftp scheme")
            {
                url.set_scheme("ftp");
                REQUIRE(url.pretty_str(URL::StripScheme::no) == "ftp://mamba.org/");
                REQUIRE(url.pretty_str(URL::StripScheme::yes) == "mamba.org/");
            }
        }

        SECTION("rstrip option")
        {
            URL url = {};
            url.set_host("mamba.org");
            REQUIRE(url.pretty_str(URL::StripScheme::no, 0) == "https://mamba.org/");
            REQUIRE(url.pretty_str(URL::StripScheme::no, '/') == "https://mamba.org");
            url.set_path("/page/");
            REQUIRE(url.pretty_str(URL::StripScheme::no, ':') == "https://mamba.org/page/");
            REQUIRE(url.pretty_str(URL::StripScheme::no, '/') == "https://mamba.org/page");
        }

        SECTION("Credential option")
        {
            URL url = {};

            SECTION("without credentials")
            {
                REQUIRE(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Show)
                    == "https://localhost/"
                );
                REQUIRE(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Hide)
                    == "https://localhost/"
                );
                REQUIRE(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Remove)
                    == "https://localhost/"
                );
            }

            SECTION("with some credentials")
            {
                url.set_user("user");
                url.set_password("pass");

                REQUIRE(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Show)
                    == "https://user:pass@localhost/"
                );
                REQUIRE(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Hide)
                    == "https://user:*****@localhost/"
                );
                REQUIRE(
                    url.pretty_str(URL::StripScheme::no, 0, URL::Credentials::Remove)
                    == "https://localhost/"
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

            REQUIRE(
                url.str() == "https://user:*****@mamba.org:8080/folder/file.html?param=value#fragment"
            );
            REQUIRE(
                url.str(URL::Credentials::Show)
                == "https://user:password@mamba.org:8080/folder/file.html?param=value#fragment"
            );
            REQUIRE(
                url.str(URL::Credentials::Hide)
                == "https://user:*****@mamba.org:8080/folder/file.html?param=value#fragment"
            );
            REQUIRE(
                url.str(URL::Credentials::Remove)
                == "https://mamba.org:8080/folder/file.html?param=value#fragment"
            );
            REQUIRE(
                url.pretty_str()
                == "https://user:*****@mamba.org:8080/folder/file.html?param=value#fragment"
            );
        }

        SECTION("user@mamba.org")
        {
            URL url{};
            url.set_host("mamba.org");
            url.set_user("user");
            REQUIRE(url.str(URL::Credentials::Show) == "https://user@mamba.org/");
            REQUIRE(url.str(URL::Credentials::Hide) == "https://user:*****@mamba.org/");
            REQUIRE(url.pretty_str() == "https://user:*****@mamba.org/");
            REQUIRE(url.pretty_str(URL::StripScheme::yes) == "user:*****@mamba.org/");
            REQUIRE(
                url.pretty_str(URL::StripScheme::yes, '\0', URL::Credentials::Hide)
                == "user:*****@mamba.org/"
            );
            REQUIRE(
                url.pretty_str(URL::StripScheme::yes, '\0', URL::Credentials::Show) == "user@mamba.org/"
            );
            REQUIRE(
                url.pretty_str(URL::StripScheme::yes, '\0', URL::Credentials::Remove) == "mamba.org/"
            );
        }

        SECTION("https://mamba.org")
        {
            URL url{};
            url.set_scheme("https");
            url.set_host("mamba.org");
            REQUIRE(url.str() == "https://mamba.org/");
            REQUIRE(url.pretty_str() == "https://mamba.org/");
            REQUIRE(url.pretty_str(URL::StripScheme::yes) == "mamba.org/");
        }

        SECTION("file:////folder/file.txt")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("//folder/file.txt");
            REQUIRE(url.str() == "file:////folder/file.txt");
            REQUIRE(url.pretty_str() == "file:////folder/file.txt");
            REQUIRE(url.pretty_str(URL::StripScheme::yes) == "//folder/file.txt");
        }

        SECTION("file:///folder/file.txt")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("/folder/file.txt");
            REQUIRE(url.str() == "file:///folder/file.txt");
            REQUIRE(url.pretty_str() == "file:///folder/file.txt");
            REQUIRE(url.pretty_str(URL::StripScheme::yes) == "/folder/file.txt");
        }

        SECTION("file:///C:/folder&/file.txt")
        {
            URL url{};
            url.set_scheme("file");
            url.set_path("C:/folder&/file.txt");
            if (on_win)
            {
                REQUIRE(url.str() == "file:///C:/folder%26/file.txt");
            }
            else
            {
                REQUIRE(url.str() == "file:///C%3A/folder%26/file.txt");
            }
            REQUIRE(url.pretty_str() == "file:///C:/folder&/file.txt");
            if (on_win)
            {
                REQUIRE(url.pretty_str(URL::StripScheme::yes) == "C:/folder&/file.txt");
            }
            else
            {
                REQUIRE(url.pretty_str(URL::StripScheme::yes) == "/C:/folder&/file.txt");
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
            REQUIRE(
                url.str(URL::Credentials::Show)
                == "https://user%40email.com:pw%25rd@mamba.org/some%20/path%24/"
            );
            REQUIRE(
                url.pretty_str(URL::StripScheme::no, '/', URL::Credentials::Show)
                == "https://user@email.com:pw%rd@mamba.org/some /path$"
            );
        }
    }

    TEST_CASE("authentication")
    {
        URL url{};
        REQUIRE(url.authentication() == "");
        url.set_user("user@email.com");
        REQUIRE(url.authentication() == "user%40email.com");
        url.set_password("password");
        REQUIRE(url.authentication() == "user%40email.com:password");
    }

    TEST_CASE("authority")
    {
        URL url{};
        url.set_scheme("https");
        url.set_host("mamba.org");
        url.set_path("/folder/file.html");
        url.set_query("param=value");
        url.set_fragment("fragment");
        REQUIRE(url.authority() == "mamba.org");
        REQUIRE(url.authority(URL::Credentials::Show) == "mamba.org");
        REQUIRE(url.authority(URL::Credentials::Hide) == "mamba.org");
        REQUIRE(url.authority(URL::Credentials::Remove) == "mamba.org");

        url.set_port("8000");
        REQUIRE(url.authority() == "mamba.org:8000");
        REQUIRE(url.authority(URL::Credentials::Show) == "mamba.org:8000");
        REQUIRE(url.authority(URL::Credentials::Hide) == "mamba.org:8000");
        REQUIRE(url.authority(URL::Credentials::Remove) == "mamba.org:8000");

        url.set_user("user@email.com");
        REQUIRE(url.authority() == "user%40email.com:*****@mamba.org:8000");
        REQUIRE(url.authority(URL::Credentials::Show) == "user%40email.com@mamba.org:8000");
        REQUIRE(url.authority(URL::Credentials::Hide) == "user%40email.com:*****@mamba.org:8000");
        REQUIRE(url.authority(URL::Credentials::Remove) == "mamba.org:8000");

        url.set_password("pass");
        REQUIRE(url.authority() == "user%40email.com:*****@mamba.org:8000");
        REQUIRE(url.authority(URL::Credentials::Show) == "user%40email.com:pass@mamba.org:8000");
        REQUIRE(url.authority(URL::Credentials::Hide) == "user%40email.com:*****@mamba.org:8000");
        REQUIRE(url.authority(URL::Credentials::Remove) == "mamba.org:8000");
    }

    TEST_CASE("Equality")
    {
        REQUIRE(URL() == URL());
        REQUIRE(
            URL::parse("https://169.254.0.0/page").value()
            == URL::parse("https://169.254.0.0/page").value()
        );
        REQUIRE(URL::parse("mamba.org").value() == URL::parse("mamba.org/").value());
        REQUIRE(URL::parse("mAmba.oRg").value() == URL::parse("mamba.org/").value());
        REQUIRE(URL::parse("localhost/page").value() == URL::parse("https://localhost/page").value());

        REQUIRE(URL::parse("mamba.org/page").value() != URL::parse("mamba.org/").value());
        REQUIRE(URL::parse("mamba.org").value() != URL::parse("mamba.org:9999").value());
        REQUIRE(URL::parse("user@mamba.org").value() != URL::parse("mamba.org").value());
        REQUIRE(URL::parse("mamba.org/page").value() != URL::parse("mamba.org/page?q=v").value());
        REQUIRE(URL::parse("mamba.org/page").value() != URL::parse("mamba.org/page#there").value());
    }

    TEST_CASE("Append path")
    {
        auto url = URL();

        SECTION("Add components")
        {
            REQUIRE(url.path() == "/");
            REQUIRE((url / "").path() == "/");
            REQUIRE((url / "   ").path() == "/   ");
            REQUIRE((url / "/").path() == "/");
            REQUIRE((url / "page").path() == "/page");
            REQUIRE((url / "/page").path() == "/page");
            REQUIRE((url / " /page").path() == "/ /page");
            REQUIRE(url.path() == "/");  // unchanged

            url.append_path("folder");
            REQUIRE(url.path() == "/folder");
            REQUIRE((url / "").path() == "/folder");
            REQUIRE((url / "/").path() == "/folder/");
            REQUIRE((url / "page").path() == "/folder/page");
            REQUIRE((url / "/page").path() == "/folder/page");
        }

        SECTION("Absolute paths")
        {
            url.set_scheme("file");
            url.append_path("C:/folder/file.txt");
            if (on_win)
            {
                REQUIRE(url.str() == "file:///C:/folder/file.txt");
            }
            else
            {
                REQUIRE(url.str() == "file:///C%3A/folder/file.txt");
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

        REQUIRE(url == other);

        SECTION("Different scheme")
        {
            other.set_scheme("ftp");
            REQUIRE(url != other);
        }

        SECTION("Different hosts")
        {
            other.clear_host();
            REQUIRE(url != other);
        }

        SECTION("Different users")
        {
            other.clear_user();
            REQUIRE(url != other);
        }

        SECTION("Different passwords")
        {
            other.clear_password();
            REQUIRE(url != other);
        }

        SECTION("Different ports")
        {
            other.clear_port();
            REQUIRE(url != other);
        }

        SECTION("Different path")
        {
            other.clear_path();
            REQUIRE(url != other);
        }
    }
}
