// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <doctest/doctest.h>

#include "mamba/specs/conda_url.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::CondaURL")
{
    TEST_CASE("Token")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SUBCASE("https://repo.mamba.pm/folder/file.txt")
        {
            url.set_path("/folder/file.txt");
            CHECK_EQ(url.token(), "");

            CHECK_THROWS_AS(url.set_token("token"), std::invalid_argument);
            CHECK_EQ(url.path(), "/folder/file.txt");

            CHECK_FALSE(url.clear_token());
            CHECK_EQ(url.path(), "/folder/file.txt");
        }

        SUBCASE("https://repo.mamba.pm/t/xy-12345678-1234/conda-forge/linux-64")
        {
            url.set_path("/t/xy-12345678-1234/conda-forge/linux-64");
            CHECK_EQ(url.token(), "xy-12345678-1234");

            SUBCASE("Cannot set invalid token")
            {
                CHECK_THROWS_AS(url.set_token(""), std::invalid_argument);
                CHECK_THROWS_AS(url.set_token("?fds:g"), std::invalid_argument);
                CHECK_EQ(url.token(), "xy-12345678-1234");
                CHECK_EQ(url.path(), "/t/xy-12345678-1234/conda-forge/linux-64");
            }

            SUBCASE("Clear token")
            {
                CHECK(url.clear_token());
                CHECK_EQ(url.token(), "");
                CHECK_EQ(url.path(), "/conda-forge/linux-64");
            }

            SUBCASE("Set token")
            {
                url.set_token("abcd");
                CHECK_EQ(url.token(), "abcd");
                CHECK_EQ(url.path(), "/t/abcd/conda-forge/linux-64");
            }
        }

        SUBCASE("https://repo.mamba.pm/t/xy-12345678-1234-1234-1234-123456789012")
        {
            url.set_path("/t/xy-12345678-1234-1234-1234-123456789012");
            CHECK_EQ(url.token(), "xy-12345678-1234-1234-1234-123456789012");

            url.set_token("abcd");
            CHECK_EQ(url.token(), "abcd");
            CHECK_EQ(url.path(), "/t/abcd");

            CHECK(url.clear_token());
            CHECK_EQ(url.path(), "/");
        }

        SUBCASE("https://repo.mamba.pm/bar/t/xy-12345678-1234-1234-1234-123456789012/")
        {
            url.set_path("/bar/t/xy-12345678-1234-1234-1234-123456789012/");
            CHECK_EQ(url.token(), "xy-12345678-1234-1234-1234-123456789012");

            url.set_token("abcd");
            CHECK_EQ(url.token(), "abcd");
            CHECK_EQ(url.path(), "/bar/t/abcd/");

            CHECK(url.clear_token());
            CHECK_EQ(url.path(), "/bar/");
        }
    }

    TEST_CASE("Platform")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SUBCASE("https://repo.mamba.pm/")
        {
            CHECK_FALSE(url.platform().has_value());
            CHECK_EQ(url.platform_name(), "");

            CHECK_THROWS_AS(url.set_platform(Platform::linux_64), std::invalid_argument);
            CHECK_EQ(url.path(), "/");

            CHECK_FALSE(url.clear_platform());
            CHECK_EQ(url.path(), "/");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge")
        {
            url.set_path("conda-forge");

            CHECK_FALSE(url.platform().has_value());
            CHECK_EQ(url.platform_name(), "");

            CHECK_THROWS_AS(url.set_platform(Platform::linux_64), std::invalid_argument);
            CHECK_EQ(url.path(), "/conda-forge");

            CHECK_FALSE(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/")
        {
            url.set_path("conda-forge/");

            CHECK_FALSE(url.platform().has_value());
            CHECK_EQ(url.platform_name(), "");

            CHECK_THROWS_AS(url.set_platform(Platform::linux_64), std::invalid_argument);
            CHECK_EQ(url.path(), "/conda-forge/");

            CHECK_FALSE(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge/");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/win-64")
        {
            url.set_path("conda-forge/win-64");

            CHECK_EQ(url.platform(), Platform::win_64);
            CHECK_EQ(url.platform_name(), "win-64");

            url.set_platform(Platform::linux_64);
            CHECK_EQ(url.platform(), Platform::linux_64);
            CHECK_EQ(url.path(), "/conda-forge/linux-64");

            CHECK(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/OSX-64/")
        {
            url.set_path("conda-forge/OSX-64");

            CHECK_EQ(url.platform(), Platform::osx_64);
            CHECK_EQ(url.platform_name(), "OSX-64");  // Captialization not changed

            url.set_platform("Win-64");
            CHECK_EQ(url.platform(), Platform::win_64);
            CHECK_EQ(url.path(), "/conda-forge/Win-64");  // Captialization not changed

            CHECK(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2")
        {
            url.set_path("/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2");

            CHECK_EQ(url.platform(), Platform::linux_64);
            CHECK_EQ(url.platform_name(), "linux-64");

            url.set_platform("osx-64");
            CHECK_EQ(url.platform(), Platform::osx_64);
            CHECK_EQ(url.path(), "/conda-forge/osx-64/micromamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge/micromamba-1.5.1-0.tar.bz2");
        }
    }

    TEST_CASE("Package")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SUBCASE("https://repo.mamba.pm/")
        {
            CHECK_EQ(url.package(), "");

            CHECK_THROWS_AS(url.set_package("not-package/"), std::invalid_argument);
            CHECK_EQ(url.path(), "/");

            CHECK_FALSE(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/");

            url.set_package("micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.package(), "micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.path(), "/micromamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge")
        {
            url.set_path("conda-forge");

            CHECK_EQ(url.package(), "");

            url.set_package("micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.package(), "micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.path(), "/conda-forge/micromamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/")
        {
            url.set_path("conda-forge/");

            CHECK_EQ(url.package(), "");

            url.set_package("micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.package(), "micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.path(), "/conda-forge/micromamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2")
        {
            url.set_path("/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2");

            CHECK_EQ(url.package(), "micromamba-1.5.1-0.tar.bz2");

            url.set_package("mamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.package(), "mamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.path(), "/conda-forge/linux-64/mamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/conda-forge/linux-64");
        }
    }

    TEST_CASE("pretty_str options")
    {
        SUBCASE("scheme option")
        {
            CondaURL url = {};
            url.set_host("mamba.org");

            SUBCASE("defaut scheme")
            {
                CHECK_EQ(url.pretty_str(CondaURL::StripScheme::no), "https://mamba.org/");
                CHECK_EQ(url.pretty_str(CondaURL::StripScheme::yes), "mamba.org/");
            }

            SUBCASE("ftp scheme")
            {
                url.set_scheme("ftp");
                CHECK_EQ(url.pretty_str(CondaURL::StripScheme::no), "ftp://mamba.org/");
                CHECK_EQ(url.pretty_str(CondaURL::StripScheme::yes), "mamba.org/");
            }
        }

        SUBCASE("rstrip option")
        {
            CondaURL url = {};
            url.set_host("mamba.org");
            CHECK_EQ(url.pretty_str(CondaURL::StripScheme::no, 0), "https://mamba.org/");
            CHECK_EQ(url.pretty_str(CondaURL::StripScheme::no, '/'), "https://mamba.org");
            url.set_path("/page/");
            CHECK_EQ(url.pretty_str(CondaURL::StripScheme::no, ':'), "https://mamba.org/page/");
            CHECK_EQ(url.pretty_str(CondaURL::StripScheme::no, '/'), "https://mamba.org/page");
        }

        SUBCASE("Hide confidential option")
        {
            CondaURL url = {};
            url.set_user("user");
            url.set_password("pass");
            CHECK_EQ(
                url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::HideConfidential::no),
                "https://user:pass@localhost/"
            );
            CHECK_EQ(
                url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::HideConfidential::yes),
                "https://user:*****@localhost/"
            );

            url.set_path("/custom/t/abcd1234/linux-64");
            CHECK_EQ(
                url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::HideConfidential::no),
                "https://user:pass@localhost/custom/t/abcd1234/linux-64"
            );

            CHECK_EQ(
                url.pretty_str(CondaURL::StripScheme::no, 0, CondaURL::HideConfidential::yes),
                "https://user:*****@localhost/custom/t/*****/linux-64"
            );
        }

        SUBCASE("https://user:password@mamba.org:8080/folder/file.html?param=value#fragment")
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

            CHECK_EQ(
                url.str(),
                "https://user:password@mamba.org:8080/folder/file.html?param=value#fragment"
            );
            CHECK_EQ(
                url.pretty_str(),
                "https://user:password@mamba.org:8080/folder/file.html?param=value#fragment"
            );
        }

        SUBCASE("https://user@email.com:pw%rd@mamba.org/some /path$/")
        {
            CondaURL url{};
            url.set_scheme("https");
            url.set_host("mamba.org");
            url.set_user("user@email.com");
            url.set_password("pw%rd");
            url.set_path("/some /path$/");
            CHECK_EQ(url.str(), "https://user%40email.com:pw%25rd@mamba.org/some%20/path%24/");
            CHECK_EQ(url.pretty_str(), "https://user@email.com:pw%rd@mamba.org/some /path$/");
        }
    }
}
