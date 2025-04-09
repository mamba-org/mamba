// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <catch2/catch_all.hpp>

#include "mamba/specs/conda_url.hpp"
#include "mamba/util/build.hpp"

using namespace mamba::specs;

namespace
{
    TEST_CASE("Token")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SECTION("https://repo.mamba.pm/folder/file.txt")
        {
            url.set_path("/folder/file.txt");
            REQUIRE_FALSE(url.has_token());
            REQUIRE(url.token() == "");
            REQUIRE(url.path_without_token() == "/folder/file.txt");

            url.set_token("mytoken");
            REQUIRE(url.has_token());
            REQUIRE(url.token() == "mytoken");
            REQUIRE(url.path_without_token() == "/folder/file.txt");
            REQUIRE(url.path() == "/t/mytoken/folder/file.txt");

            REQUIRE(url.clear_token());
            REQUIRE_FALSE(url.has_token());
            REQUIRE(url.path_without_token() == "/folder/file.txt");
            REQUIRE(url.path() == "/folder/file.txt");
        }

        SECTION("https://repo.mamba.pm/t/xy-12345678-1234/conda-forge/linux-64")
        {
            url.set_path("/t/xy-12345678-1234/conda-forge/linux-64");
            REQUIRE(url.has_token());
            REQUIRE(url.token() == "xy-12345678-1234");
            REQUIRE(url.path_without_token() == "/conda-forge/linux-64");

            SECTION("Cannot set invalid token")
            {
                REQUIRE_THROWS_AS(url.set_token(""), std::invalid_argument);
                REQUIRE_THROWS_AS(url.set_token("?fds:g"), std::invalid_argument);
                REQUIRE(url.has_token());
                REQUIRE(url.token() == "xy-12345678-1234");
                REQUIRE(url.path_without_token() == "/conda-forge/linux-64");
                REQUIRE(url.path() == "/t/xy-12345678-1234/conda-forge/linux-64");
            }

            SECTION("Clear token")
            {
                REQUIRE(url.clear_token());
                REQUIRE_FALSE(url.has_token());
                REQUIRE(url.token() == "");
                REQUIRE(url.path_without_token() == "/conda-forge/linux-64");
                REQUIRE(url.path() == "/conda-forge/linux-64");
            }

            SECTION("Set token")
            {
                url.set_token("abcd");
                REQUIRE(url.has_token());
                REQUIRE(url.token() == "abcd");
                REQUIRE(url.path_without_token() == "/conda-forge/linux-64");
                REQUIRE(url.path() == "/t/abcd/conda-forge/linux-64");
            }
        }

        SECTION("https://repo.mamba.pm/t/xy-12345678-1234-1234-1234-123456789012")
        {
            url.set_path("/t/xy-12345678-1234-1234-1234-123456789012");
            REQUIRE(url.has_token());
            REQUIRE(url.token() == "xy-12345678-1234-1234-1234-123456789012");

            url.set_token("abcd");
            REQUIRE(url.has_token());
            REQUIRE(url.token() == "abcd");
            REQUIRE(url.path_without_token() == "/");
            REQUIRE(url.path() == "/t/abcd/");

            REQUIRE(url.clear_token());
            REQUIRE_FALSE(url.has_token());
            REQUIRE(url.token() == "");
            REQUIRE(url.path_without_token() == "/");
            REQUIRE(url.path() == "/");
        }

        SECTION("https://repo.mamba.pm/bar/t/xy-12345678-1234-1234-1234-123456789012/")
        {
            url.set_path("/bar/t/xy-12345678-1234-1234-1234-123456789012/");
            REQUIRE_FALSE(url.has_token());
            REQUIRE(url.token() == "");  // Not at beginning of path

            url.set_token("abcd");
            REQUIRE(url.has_token());
            REQUIRE(url.token() == "abcd");
            REQUIRE(url.path_without_token() == "/bar/t/xy-12345678-1234-1234-1234-123456789012/");
            REQUIRE(url.path() == "/t/abcd/bar/t/xy-12345678-1234-1234-1234-123456789012/");

            REQUIRE(url.clear_token());
            REQUIRE(url.path_without_token() == "/bar/t/xy-12345678-1234-1234-1234-123456789012/");
            REQUIRE(url.path() == "/bar/t/xy-12345678-1234-1234-1234-123456789012/");
        }
    }

    TEST_CASE("Path without token")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SECTION("Setters")
        {
            url.set_path_without_token("foo");
            REQUIRE(url.path_without_token() == "/foo");
            url.set_token("mytoken");
            REQUIRE(url.path_without_token() == "/foo");
            REQUIRE(url.clear_path_without_token());
            REQUIRE(url.path_without_token() == "/");
        }

        SECTION("Parse")
        {
            url = CondaURL::parse("mamba.org/t/xy-12345678-1234-1234-1234-123456789012").value();
            REQUIRE(url.has_token());
            REQUIRE(url.token() == "xy-12345678-1234-1234-1234-123456789012");
            REQUIRE(url.path_without_token() == "/");
            REQUIRE(url.path() == "/t/xy-12345678-1234-1234-1234-123456789012/");
        }

        SECTION("Encoding")
        {
            url.set_token("mytoken");

            SECTION("Encode")
            {
                url.set_path_without_token("some / weird/path %");
                REQUIRE(url.path_without_token() == "/some / weird/path %");
                REQUIRE(url.path_without_token(CondaURL::Decode::no) == "/some%20/%20weird/path%20%25");
            }

            SECTION("Encoded")
            {
                url.set_path_without_token("/some%20/%20weird/path%20%25", CondaURL::Encode::no);
                REQUIRE(url.path_without_token() == "/some / weird/path %");
                REQUIRE(url.path_without_token(CondaURL::Decode::no) == "/some%20/%20weird/path%20%25");
            }
        }
    }

    TEST_CASE("Platform")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SECTION("https://repo.mamba.pm/")
        {
            REQUIRE_FALSE(url.platform().has_value());
            REQUIRE(url.platform_name() == "");

            REQUIRE_THROWS_AS(url.set_platform(KnownPlatform::linux_64), std::invalid_argument);
            REQUIRE(url.path_without_token() == "/");
            REQUIRE(url.path() == "/");

            REQUIRE_FALSE(url.clear_platform());
            REQUIRE(url.path() == "/");
        }

        SECTION("https://repo.mamba.pm/conda-forge")
        {
            url.set_path("conda-forge");

            REQUIRE_FALSE(url.platform().has_value());
            REQUIRE(url.platform_name() == "");

            REQUIRE_THROWS_AS(url.set_platform(KnownPlatform::linux_64), std::invalid_argument);
            REQUIRE(url.path() == "/conda-forge");

            REQUIRE_FALSE(url.clear_platform());
            REQUIRE(url.path() == "/conda-forge");
        }

        SECTION("https://repo.mamba.pm/conda-forge/")
        {
            url.set_path("conda-forge/");

            REQUIRE_FALSE(url.platform().has_value());
            REQUIRE(url.platform_name() == "");

            REQUIRE_THROWS_AS(url.set_platform(KnownPlatform::linux_64), std::invalid_argument);
            REQUIRE(url.path() == "/conda-forge/");

            REQUIRE_FALSE(url.clear_platform());
            REQUIRE(url.path() == "/conda-forge/");
        }

        SECTION("https://repo.mamba.pm/conda-forge/win-64")
        {
            url.set_path("conda-forge/win-64");

            REQUIRE(url.platform() == KnownPlatform::win_64);
            REQUIRE(url.platform_name() == "win-64");

            url.set_platform(KnownPlatform::linux_64);
            REQUIRE(url.platform() == KnownPlatform::linux_64);
            REQUIRE(url.path() == "/conda-forge/linux-64");

            REQUIRE(url.clear_platform());
            REQUIRE(url.path() == "/conda-forge");
        }

        SECTION("https://repo.mamba.pm/conda-forge/OSX-64/")
        {
            url.set_path("conda-forge/OSX-64");

            REQUIRE(url.platform() == KnownPlatform::osx_64);
            REQUIRE(url.platform_name() == "OSX-64");  // Capitalization not changed

            url.set_platform("Win-64");
            REQUIRE(url.platform() == KnownPlatform::win_64);
            REQUIRE(url.path() == "/conda-forge/Win-64");  // Capitalization not changed

            REQUIRE(url.clear_platform());
            REQUIRE(url.path() == "/conda-forge");
        }

        SECTION("https://repo.mamba.pm/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2")
        {
            url.set_path("/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2");

            REQUIRE(url.platform() == KnownPlatform::linux_64);
            REQUIRE(url.platform_name() == "linux-64");

            url.set_platform("osx-64");
            REQUIRE(url.platform() == KnownPlatform::osx_64);
            REQUIRE(url.path() == "/conda-forge/osx-64/micromamba-1.5.1-0.tar.bz2");

            REQUIRE(url.clear_platform());
            REQUIRE(url.path() == "/conda-forge/micromamba-1.5.1-0.tar.bz2");
        }
    }

    TEST_CASE("Package")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SECTION("https://repo.mamba.pm/")
        {
            REQUIRE(url.package() == "");

            REQUIRE_THROWS_AS(url.set_package("not-package/"), std::invalid_argument);
            REQUIRE(url.path() == "/");

            REQUIRE_FALSE(url.clear_package());
            REQUIRE(url.package() == "");
            REQUIRE(url.path() == "/");

            url.set_package("micromamba-1.5.1-0.tar.bz2");
            REQUIRE(url.package() == "micromamba-1.5.1-0.tar.bz2");
            REQUIRE(url.path() == "/micromamba-1.5.1-0.tar.bz2");

            REQUIRE(url.clear_package());
            REQUIRE(url.package() == "");
            REQUIRE(url.path() == "/");
        }

        SECTION("https://repo.mamba.pm/conda-forge")
        {
            url.set_path("conda-forge");

            REQUIRE(url.package() == "");

            url.set_package("micromamba-1.5.1-0.tar.bz2");
            REQUIRE(url.package() == "micromamba-1.5.1-0.tar.bz2");
            REQUIRE(url.path() == "/conda-forge/micromamba-1.5.1-0.tar.bz2");

            REQUIRE(url.clear_package());
            REQUIRE(url.package() == "");
            REQUIRE(url.path() == "/conda-forge");
        }

        SECTION("https://repo.mamba.pm/conda-forge/")
        {
            url.set_path("conda-forge/");

            REQUIRE(url.package() == "");

            url.set_package("micromamba-1.5.1-0.tar.bz2");
            REQUIRE(url.package() == "micromamba-1.5.1-0.tar.bz2");
            REQUIRE(url.path() == "/conda-forge/micromamba-1.5.1-0.tar.bz2");

            REQUIRE(url.clear_package());
            REQUIRE(url.package() == "");
            REQUIRE(url.path() == "/conda-forge");
        }

        SECTION("https://repo.mamba.pm/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2")
        {
            url.set_path("/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2");

            REQUIRE(url.package() == "micromamba-1.5.1-0.tar.bz2");

            url.set_package("mamba-1.5.1-0.tar.bz2");
            REQUIRE(url.package() == "mamba-1.5.1-0.tar.bz2");
            REQUIRE(url.path() == "/conda-forge/linux-64/mamba-1.5.1-0.tar.bz2");

            REQUIRE(url.clear_package());
            REQUIRE(url.package() == "");
            REQUIRE(url.path() == "/conda-forge/linux-64");
        }
    }

    TEST_CASE("CondaURL str options")
    {
        CondaURL url = {};

        SECTION("without credentials")
        {
            REQUIRE(url.str(CondaURL::Credentials::Show) == "https://localhost/");
            REQUIRE(url.str(CondaURL::Credentials::Hide) == "https://localhost/");
            REQUIRE(url.str(CondaURL::Credentials::Remove) == "https://localhost/");
        }

        SECTION("with some credentials")
        {
            url.set_user("user@mamba.org");
            url.set_password("pass");

            REQUIRE(url.str(CondaURL::Credentials::Show) == "https://user%40mamba.org:pass@localhost/");
            REQUIRE(
                url.str(CondaURL::Credentials::Hide) == "https://user%40mamba.org:*****@localhost/"
            );
            REQUIRE(url.str(CondaURL::Credentials::Remove) == "https://localhost/");

            SECTION("and token")
            {
                url.set_path("/t/abcd1234/linux-64");
                REQUIRE(
                    url.str(CondaURL::Credentials::Show)
                    == "https://user%40mamba.org:pass@localhost/t/abcd1234/linux-64"
                );
                REQUIRE(
                    url.str(CondaURL::Credentials::Hide)
                    == "https://user%40mamba.org:*****@localhost/t/*****/linux-64"
                );
                REQUIRE(url.str(CondaURL::Credentials::Remove) == "https://localhost/linux-64");
            }
        }
    }

    TEST_CASE("CondaURL pretty_str options")
    {
        SECTION("scheme option")
        {
            CondaURL url = {};
            url.set_host("mamba.org");

            SECTION("default scheme")
            {
                REQUIRE(url.pretty_str(CondaURL::StripScheme::no) == "https://mamba.org/");
                REQUIRE(url.pretty_str(CondaURL::StripScheme::yes) == "mamba.org/");
            }

            SECTION("ftp scheme")
            {
                url.set_scheme("ftp");
                REQUIRE(url.pretty_str(CondaURL::StripScheme::no) == "ftp://mamba.org/");
                REQUIRE(url.pretty_str(CondaURL::StripScheme::yes) == "mamba.org/");
            }
        }

        SECTION("rstrip option")
        {
            CondaURL url = {};
            url.set_host("mamba.org");
            REQUIRE(url.pretty_str(CondaURL::StripScheme::no, 0) == "https://mamba.org/");
            REQUIRE(url.pretty_str(CondaURL::StripScheme::no, '/') == "https://mamba.org");
            url.set_path("/page/");
            REQUIRE(url.pretty_str(CondaURL::StripScheme::no, ':') == "https://mamba.org/page/");
            REQUIRE(url.pretty_str(CondaURL::StripScheme::no, '/') == "https://mamba.org/page");
        }

        SECTION("Credential option")
        {
            CondaURL url = {};

            SECTION("without credentials")
            {
                REQUIRE(
                    url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::Credentials::Show)
                    == "https://localhost/"
                );
                REQUIRE(
                    url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::Credentials::Hide)
                    == "https://localhost/"
                );
                REQUIRE(
                    url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::Credentials::Remove)
                    == "https://localhost/"
                );
            }

            SECTION("with user:password")
            {
                url.set_user("user");
                url.set_password("pass");
                REQUIRE(
                    url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::Credentials::Show)
                    == "https://user:pass@localhost/"
                );
                REQUIRE(
                    url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::Credentials::Hide)
                    == "https://user:*****@localhost/"
                );
                REQUIRE(
                    url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::Credentials::Remove)
                    == "https://localhost/"
                );

                SECTION("and token")
                {
                    url.set_path("/t/abcd1234/linux-64");
                    REQUIRE(
                        url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::Credentials::Show)
                        == "https://user:pass@localhost/t/abcd1234/linux-64"
                    );
                    REQUIRE(
                        url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::Credentials::Hide)
                        == "https://user:*****@localhost/t/*****/linux-64"
                    );
                    REQUIRE(
                        url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::Credentials::Remove)
                        == "https://localhost/linux-64"
                    );
                }
            }
        }

        SECTION("https://user:password@mamba.org:8080/folder/file.html?param=value#fragment")
        {
            CondaURL url{};
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
                url.str(CondaURL::Credentials::Show)
                == "https://user:password@mamba.org:8080/folder/file.html?param=value#fragment"
            );
            REQUIRE(
                url.pretty_str()
                == "https://user:*****@mamba.org:8080/folder/file.html?param=value#fragment"
            );
        }

        SECTION("https://user@email.com:pw%rd@mamba.org/some /path$/")
        {
            CondaURL url{};
            url.set_scheme("https");
            url.set_host("mamba.org");
            url.set_user("user@email.com");
            url.set_password("pw%rd");
            url.set_path("/some /path$/");
            REQUIRE(url.str() == "https://user%40email.com:*****@mamba.org/some%20/path%24/");
            REQUIRE(
                url.str(CondaURL::Credentials::Show)
                == "https://user%40email.com:pw%25rd@mamba.org/some%20/path%24/"
            );
            REQUIRE(url.pretty_str() == "https://user@email.com:*****@mamba.org/some /path$/");
        }
    }

    TEST_CASE("CondaURL::parse")
    {
        SECTION("file:////D:/a/_temp/popen-gw0/some_other_parts")
        {
            auto url = CondaURL::parse("file:////D:/a/_temp/popen-gw0/some_other_parts").value();
            REQUIRE(url.path() == "//D:/a/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.str() == "file:////D:/a/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.pretty_str() == "file:////D:/a/_temp/popen-gw0/some_other_parts");
        }

        SECTION("file:////ab/_temp/popen-gw0/some_other_parts")
        {
            auto url = CondaURL::parse("file:////ab/_temp/popen-gw0/some_other_parts").value();
            REQUIRE(url.path() == "//ab/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.str() == "file:////ab/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.pretty_str() == "file:////ab/_temp/popen-gw0/some_other_parts");
        }

        SECTION("file:///D:/a/_temp/popen-gw0/some_other_parts")
        {
            auto url = CondaURL::parse("file:///D:/a/_temp/popen-gw0/some_other_parts").value();
            if (mamba::util::on_win)
            {
                REQUIRE(url.path() == "/D:/a/_temp/popen-gw0/some_other_parts");
                REQUIRE(url.str() == "file:///D:/a/_temp/popen-gw0/some_other_parts");
                REQUIRE(url.pretty_str() == "file:///D:/a/_temp/popen-gw0/some_other_parts");
            }
            else
            {
                REQUIRE(url.path() == "//D:/a/_temp/popen-gw0/some_other_parts");
                REQUIRE(url.str() == "file:////D:/a/_temp/popen-gw0/some_other_parts");
                REQUIRE(url.pretty_str() == "file:////D:/a/_temp/popen-gw0/some_other_parts");
            }
        }

        SECTION("file:///ab/_temp/popen-gw0/some_other_parts")
        {
            auto url = CondaURL::parse("file:///ab/_temp/popen-gw0/some_other_parts").value();
            REQUIRE(url.path() == "/ab/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.str() == "file:///ab/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.pretty_str() == "file:///ab/_temp/popen-gw0/some_other_parts");
        }

        SECTION("file://D:/a/_temp/popen-gw0/some_other_parts")
        {
            auto url = CondaURL::parse("file://D:/a/_temp/popen-gw0/some_other_parts").value();
            if (mamba::util::on_win)
            {
                REQUIRE(url.path() == "/D:/a/_temp/popen-gw0/some_other_parts");
                REQUIRE(url.str() == "file:///D:/a/_temp/popen-gw0/some_other_parts");
                REQUIRE(url.pretty_str() == "file:///D:/a/_temp/popen-gw0/some_other_parts");
            }
            else
            {
                REQUIRE(url.path() == "//D:/a/_temp/popen-gw0/some_other_parts");
                REQUIRE(url.str() == "file:////D:/a/_temp/popen-gw0/some_other_parts");
                REQUIRE(url.pretty_str() == "file:////D:/a/_temp/popen-gw0/some_other_parts");
            }
        }

        SECTION("file://ab/_temp/popen-gw0/some_other_parts")
        {
            auto url = CondaURL::parse("file://ab/_temp/popen-gw0/some_other_parts").value();
            REQUIRE(url.path() == "//ab/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.str() == "file:////ab/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.pretty_str() == "file:////ab/_temp/popen-gw0/some_other_parts");
        }

        // NOTE This is not valid on any platform:
        // R"(file://\\D:/a/_temp/popen-gw0/some_other_parts)"
        SECTION("file://\\D:/a/_temp/popen-gw0/some_other_parts")
        {
            auto url = CondaURL::parse("file://\\D:/a/_temp/popen-gw0/some_other_parts").value();
            REQUIRE(url.path() == "file://\\D:/a/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.str() == "file://\\D:/a/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.pretty_str() == "file://\\D:/a/_temp/popen-gw0/some_other_parts");
        }

        SECTION("file://\\abcd/_temp/popen-gw0/some_other_parts")
        {
            auto url = CondaURL::parse("file://\\abcd/_temp/popen-gw0/some_other_parts").value();
            REQUIRE(url.path() == "//\\abcd/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.str() == "file:////\\abcd/_temp/popen-gw0/some_other_parts");
            REQUIRE(url.pretty_str() == "file:////\\abcd/_temp/popen-gw0/some_other_parts");
        }
    }
}
