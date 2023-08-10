// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/url.hpp"

#ifdef _WIN32
#include "mamba/core/mamba_fs.hpp"
#endif

using namespace mamba;

TEST_SUITE("url")
{
    TEST_CASE("concat_scheme_url")
    {
        auto url = concat_scheme_url("https", "mamba.com");
        CHECK_EQ(url, "https://mamba.com");

        url = concat_scheme_url("file", "C:/some_folder");
        CHECK_EQ(url, "file:///C:/some_folder");

        url = concat_scheme_url("file", "some_folder");
        CHECK_EQ(url, "file://some_folder");
    }

    TEST_CASE("build_url")
    {
        auto url = build_url(std::nullopt, "https", "mamba.com", true);
        CHECK_EQ(url, "https://mamba.com");

        url = build_url(std::nullopt, "https", "mamba.com", false);
        CHECK_EQ(url, "https://mamba.com");

        url = build_url(std::optional<std::string>("auth"), "https", "mamba.com", false);
        CHECK_EQ(url, "https://mamba.com");

        url = build_url(std::optional<std::string>("auth"), "https", "mamba.com", true);
        CHECK_EQ(url, "https://auth@mamba.com");

        url = build_url(std::optional<std::string>(""), "https", "mamba.com", true);
        CHECK_EQ(url, "https://@mamba.com");
    }

    TEST_CASE("split_platform")
    {
        auto& ctx = Context::instance();

        std::string platform_found, cleaned_url;
        split_platform(
            { "noarch", "linux-64" },
            "https://mamba.com/linux-64/package.tar.bz2",
            ctx.platform,
            cleaned_url,
            platform_found
        );

        CHECK_EQ(platform_found, "linux-64");
        CHECK_EQ(cleaned_url, "https://mamba.com/package.tar.bz2");

        split_platform(
            { "noarch", "linux-64" },
            "https://mamba.com/linux-64/noarch-package.tar.bz2",
            ctx.platform,
            cleaned_url,
            platform_found
        );
        CHECK_EQ(platform_found, "linux-64");
        CHECK_EQ(cleaned_url, "https://mamba.com/noarch-package.tar.bz2");

        split_platform(
            { "linux-64", "osx-arm64", "noarch" },
            "https://mamba.com/noarch/kernel_linux-64-package.tar.bz2",
            ctx.platform,
            cleaned_url,
            platform_found
        );
        CHECK_EQ(platform_found, "noarch");
        CHECK_EQ(cleaned_url, "https://mamba.com/kernel_linux-64-package.tar.bz2");

        split_platform(
            { "noarch", "linux-64" },
            "https://mamba.com/linux-64",
            ctx.platform,
            cleaned_url,
            platform_found
        );

        CHECK_EQ(platform_found, "linux-64");
        CHECK_EQ(cleaned_url, "https://mamba.com");

        split_platform(
            { "noarch", "linux-64" },
            "https://mamba.com/noarch",
            ctx.platform,
            cleaned_url,
            platform_found
        );

        CHECK_EQ(platform_found, "noarch");
        CHECK_EQ(cleaned_url, "https://mamba.com");
    }

    TEST_CASE("parse")
    {
        SUBCASE("http://mamba.org")
        {
            URL url("http://mamba.org");
            CHECK_EQ(url.scheme(), "http");
            CHECK_EQ(url.host(), "mamba.org");
            CHECK_EQ(url.user(), "");
            CHECK_EQ(url.password(), "");
            CHECK_EQ(url.port(), "");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.query(), "");
        }

        SUBCASE("s3://userx123:üúßsajd@mamba.org")
        {
            URL url("s3://userx123:üúßsajd@mamba.org");
            CHECK_EQ(url.scheme(), "s3");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.host(), "mamba.org");
            CHECK_EQ(url.user(), "userx123");
            CHECK_EQ(url.password(), "üúßsajd");
        }

        SUBCASE("http://user%40email.com:test@localhost:8000")
        {
            URL url("http://user%40email.com:test@localhost:8000");
            CHECK_EQ(url.scheme(), "http");
            CHECK_EQ(url.path(), "/");
            CHECK_EQ(url.host(), "localhost");
            CHECK_EQ(url.port(), "8000");
            CHECK_EQ(url.user(), "user%40email.com");
            CHECK_EQ(url.password(), "test");
        }

        SUBCASE("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333")
        {
            URL url("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333");
            CHECK_EQ(url.scheme(), "https");
            CHECK_EQ(url.path(), "/this/is/a/path/");
            CHECK_EQ(url.host(), "mamba🆒🔬.org");
            CHECK_EQ(url.query(), "query=123&xyz=3333");
        }

        SUBCASE("file://C:/Users/wolfv/test/document.json")
        {
#ifdef _WIN32
            URL url("file://C:/Users/wolfv/test/document.json");
            CHECK_EQ(url.scheme(), "file");
            CHECK_EQ(url.path(), "C:/Users/wolfv/test/document.json");
#endif
        }

        SUBCASE("file:///home/wolfv/test/document.json")
        {
#ifndef _WIN32
            URL url("file:///home/wolfv/test/document.json");
            CHECK_EQ(url.scheme(), "file");
            CHECK_EQ(url.path(), "/home/wolfv/test/document.json");
#endif
        }
    }

    TEST_CASE("path_to_url")
    {
        auto url = path_to_url("/users/test/miniconda3");
#ifndef _WIN32
        CHECK_EQ(url, "file:///users/test/miniconda3");
#else
        std::string driveletter = fs::absolute(fs::u8path("/")).string().substr(0, 1);
        CHECK_EQ(url, std::string("file://") + driveletter + ":/users/test/miniconda3");
        auto url2 = path_to_url("D:\\users\\test\\miniconda3");
        CHECK_EQ(url2, "file://D:/users/test/miniconda3");
#endif
    }

    TEST_CASE("file_uri_unc2_to_unc4")
    {
        for (std::string const uri : {
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

    TEST_CASE("has_scheme")
    {
        std::string url = "http://mamba.org";
        std::string not_url = "mamba.org";

        CHECK(has_scheme(url));
        CHECK_FALSE(has_scheme(not_url));
        CHECK_FALSE(has_scheme(""));
    }

    TEST_CASE("split_ananconda_token")
    {
        std::string input, cleaned_url, token;
        {
            input = "https://1.2.3.4/t/tk-123-456/path";
            split_anaconda_token(input, cleaned_url, token);
            CHECK_EQ(cleaned_url, "https://1.2.3.4/path");
            CHECK_EQ(token, "tk-123-456");
        }

        {
            input = "https://1.2.3.4/t//path";
            split_anaconda_token(input, cleaned_url, token);
            CHECK_EQ(cleaned_url, "https://1.2.3.4/path");
            CHECK_EQ(token, "");
        }

        {
            input = "https://some.domain/api/t/tk-123-456/path";
            split_anaconda_token(input, cleaned_url, token);
            CHECK_EQ(cleaned_url, "https://some.domain/api/path");
            CHECK_EQ(token, "tk-123-456");
        }

        {
            input = "https://1.2.3.4/conda/t/tk-123-456/path";
            split_anaconda_token(input, cleaned_url, token);
            CHECK_EQ(cleaned_url, "https://1.2.3.4/conda/path");
            CHECK_EQ(token, "tk-123-456");
        }

        {
            input = "https://1.2.3.4/path";
            split_anaconda_token(input, cleaned_url, token);
            CHECK_EQ(cleaned_url, "https://1.2.3.4/path");
            CHECK_EQ(token, "");
        }

        {
            input = "https://10.2.3.4:8080/conda/t/tk-123-45";
            split_anaconda_token(input, cleaned_url, token);
            CHECK_EQ(cleaned_url, "https://10.2.3.4:8080/conda");
            CHECK_EQ(token, "tk-123-45");
        }
    }

    TEST_CASE("split_scheme_auth_token")
    {
        std::string input = "https://u:p@conda.io/t/x1029384756/more/path";
        std::string input2 = "https://u:p@conda.io/t/a_-12345-absdj12345-xyxyxyx/more/path";
        std::string remaining_url, scheme, auth, token;
        split_scheme_auth_token(input, remaining_url, scheme, auth, token);
        CHECK_EQ(remaining_url, "conda.io/more/path");
        CHECK_EQ(scheme, "https");
        CHECK_EQ(auth, "u:p");
        CHECK_EQ(token, "x1029384756");

        split_scheme_auth_token(input2, remaining_url, scheme, auth, token);
        CHECK_EQ(remaining_url, "conda.io/more/path");
        CHECK_EQ(scheme, "https");
        CHECK_EQ(auth, "u:p");
        CHECK_EQ(token, "a_-12345-absdj12345-xyxyxyx");

#ifdef _WIN32
        split_scheme_auth_token("file://C:/Users/wolfv/test.json", remaining_url, scheme, auth, token);
        CHECK_EQ(remaining_url, "C:/Users/wolfv/test.json");
        CHECK_EQ(scheme, "file");
        CHECK_EQ(auth, "");
        CHECK_EQ(token, "");
#else
        split_scheme_auth_token("file:///home/wolfv/test.json", remaining_url, scheme, auth, token);
        CHECK_EQ(remaining_url, "/home/wolfv/test.json");
        CHECK_EQ(scheme, "file");
        CHECK_EQ(auth, "");
        CHECK_EQ(token, "");
#endif
    }

    TEST_CASE("is_path")
    {
        CHECK(is_path("./"));
        CHECK(is_path(".."));
        CHECK(is_path("~"));
        CHECK(is_path("/"));
        CHECK_FALSE(is_path("file://makefile"));
    }

    TEST_CASE("cache_name_from_url")
    {
        CHECK_EQ(cache_name_from_url("http://test.com/1234/"), "302f0a61");
        CHECK_EQ(cache_name_from_url("http://test.com/1234/repodata.json"), "302f0a61");
        CHECK_EQ(cache_name_from_url("http://test.com/1234/current_repodata.json"), "78a8cce9");
    }
}
