#include <gtest/gtest.h>

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/url.hpp"

namespace mamba
{
    const std::vector<std::string> KNOWN_PLATFORMS
        = { "noarch",       "linux-32",      "linux-64",    "linux-aarch64", "linux-armv6l",
            "linux-armv7l", "linux-ppc64le", "linux-ppc64", "osx-64",        "osx-arm64",
            "win-32",       "win-64",        "zos-z" };

    TEST(url, parse)
    {
        {
            URLHandler m("http://mamba.org");
            EXPECT_EQ(m.scheme(), "http");
            EXPECT_EQ(m.path(), "/");
            EXPECT_EQ(m.host(), "mamba.org");
        }
        {
            URLHandler m("s3://userx123:üúßsajd@mamba.org");
            EXPECT_EQ(m.scheme(), "s3");
            EXPECT_EQ(m.path(), "/");
            EXPECT_EQ(m.host(), "mamba.org");
            EXPECT_EQ(m.user(), "userx123");
            EXPECT_EQ(m.password(), "üúßsajd");
        }
        {
            URLHandler m("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333");
            EXPECT_EQ(m.scheme(), "https");
            EXPECT_EQ(m.path(), "/this/is/a/path/");
            EXPECT_EQ(m.host(), "mamba🆒🔬.org");
            EXPECT_EQ(m.query(), "query=123&xyz=3333");
        }
        {
#ifdef _WIN32
            URLHandler m("file://C:/Users/wolfv/test/document.json");
            EXPECT_EQ(m.scheme(), "file");
            EXPECT_EQ(m.path(), "C:/Users/wolfv/test/document.json");
#else
            URLHandler m("file:///home/wolfv/test/document.json");
            EXPECT_EQ(m.scheme(), "file");
            EXPECT_EQ(m.path(), "/home/wolfv/test/document.json");
#endif
        }
    }

    TEST(url, path_to_url)
    {
        auto url = path_to_url("/users/test/miniconda3");
#ifndef _WIN32
        EXPECT_EQ(url, "file:///users/test/miniconda3");
#else
        std::string driveletter = fs::absolute(fs::path("/")).string().substr(0, 1);
        EXPECT_EQ(url, std::string("file://") + driveletter + ":/users/test/miniconda3");
        auto url2 = path_to_url("D:\\users\\test\\miniconda3");
        EXPECT_EQ(url2, "file://D:/users/test/miniconda3");
#endif
    }

    TEST(url, unc_url)
    {
        {
            auto out = unc_url("http://example.com/test");
            EXPECT_EQ(out, "http://example.com/test");
        }
        {
            auto out = unc_url("file://C:/Program\\ (x74)/Users/hello\\ world");
            EXPECT_EQ(out, "file://C:/Program\\ (x74)/Users/hello\\ world");
        }
        {
            auto out = unc_url("file:///C:/Program\\ (x74)/Users/hello\\ world");
            EXPECT_EQ(out, "file:///C:/Program\\ (x74)/Users/hello\\ world");
        }
        {
            auto out = unc_url("file:////server/share");
            EXPECT_EQ(out, "file:////server/share");
        }
        {
            auto out = unc_url("file:///absolute/path");
            EXPECT_EQ(out, "file:///absolute/path");
        }
        {
            auto out = unc_url("file://server/share");
            EXPECT_EQ(out, "file:////server/share");
        }
        {
            auto out = unc_url("file://server");
            EXPECT_EQ(out, "file:////server");
        }
    }

    TEST(url, has_scheme)
    {
        std::string url = "http://mamba.org";
        std::string not_url = "mamba.org";

        EXPECT_TRUE(has_scheme(url));
        EXPECT_FALSE(has_scheme(not_url));
        EXPECT_FALSE(has_scheme(""));
    }

    TEST(url, value_semantic)
    {
        {
            URLHandler in("s3://userx123:üúßsajd@mamba.org");
            URLHandler m(in);
            EXPECT_EQ(m.scheme(), "s3");
            EXPECT_EQ(m.path(), "/");
            EXPECT_EQ(m.host(), "mamba.org");
            EXPECT_EQ(m.user(), "userx123");
            EXPECT_EQ(m.password(), "üúßsajd");
        }

        {
            URLHandler m("http://mamba.org");
            URLHandler in("s3://userx123:üúßsajd@mamba.org");
            m = in;
            EXPECT_EQ(m.scheme(), "s3");
            EXPECT_EQ(m.path(), "/");
            EXPECT_EQ(m.host(), "mamba.org");
            EXPECT_EQ(m.user(), "userx123");
            EXPECT_EQ(m.password(), "üúßsajd");
        }

        {
            URLHandler in("s3://userx123:üúßsajd@mamba.org");
            URLHandler m(std::move(in));
            EXPECT_EQ(m.scheme(), "s3");
            EXPECT_EQ(m.path(), "/");
            EXPECT_EQ(m.host(), "mamba.org");
            EXPECT_EQ(m.user(), "userx123");
            EXPECT_EQ(m.password(), "üúßsajd");
        }

        {
            URLHandler m("http://mamba.org");
            URLHandler in("s3://userx123:üúßsajd@mamba.org");
            m = std::move(in);
            EXPECT_EQ(m.scheme(), "s3");
            EXPECT_EQ(m.path(), "/");
            EXPECT_EQ(m.host(), "mamba.org");
            EXPECT_EQ(m.user(), "userx123");
            EXPECT_EQ(m.password(), "üúßsajd");
        }
    }

    TEST(url, split_ananconda_token)
    {
        std::string input, cleaned_url, token;
        {
            input = "https://1.2.3.4/t/tk-123-456/path";
            split_anaconda_token(input, cleaned_url, token);
            EXPECT_EQ(cleaned_url, "https://1.2.3.4/path");
            EXPECT_EQ(token, "tk-123-456");
        }

        {
            input = "https://1.2.3.4/t//path";
            split_anaconda_token(input, cleaned_url, token);
            EXPECT_EQ(cleaned_url, "https://1.2.3.4/path");
            EXPECT_EQ(token, "");
        }

        {
            input = "https://some.domain/api/t/tk-123-456/path";
            split_anaconda_token(input, cleaned_url, token);
            EXPECT_EQ(cleaned_url, "https://some.domain/api/path");
            EXPECT_EQ(token, "tk-123-456");
        }

        {
            input = "https://1.2.3.4/conda/t/tk-123-456/path";
            split_anaconda_token(input, cleaned_url, token);
            EXPECT_EQ(cleaned_url, "https://1.2.3.4/conda/path");
            EXPECT_EQ(token, "tk-123-456");
        }

        {
            input = "https://1.2.3.4/path";
            split_anaconda_token(input, cleaned_url, token);
            EXPECT_EQ(cleaned_url, "https://1.2.3.4/path");
            EXPECT_EQ(token, "");
        }

        {
            input = "https://10.2.3.4:8080/conda/t/tk-123-45";
            split_anaconda_token(input, cleaned_url, token);
            EXPECT_EQ(cleaned_url, "https://10.2.3.4:8080/conda");
            EXPECT_EQ(token, "tk-123-45");
        }
    }

    TEST(url, split_scheme_auth_token)
    {
        std::string input = "https://u:p@conda.io/t/x1029384756/more/path";
        std::string remaining_url, scheme, auth, token;
        split_scheme_auth_token(input, remaining_url, scheme, auth, token);
        EXPECT_EQ(remaining_url, "conda.io/more/path");
        EXPECT_EQ(scheme, "https");
        EXPECT_EQ(auth, "u:p");
        EXPECT_EQ(token, "x1029384756");

#ifdef _WIN32
        split_scheme_auth_token(
            "file://C:/Users/wolfv/test.json", remaining_url, scheme, auth, token);
        EXPECT_EQ(remaining_url, "C:/Users/wolfv/test.json");
        EXPECT_EQ(scheme, "file");
        EXPECT_EQ(auth, "");
        EXPECT_EQ(token, "");
#else
        split_scheme_auth_token("file:///home/wolfv/test.json", remaining_url, scheme, auth, token);
        EXPECT_EQ(remaining_url, "/home/wolfv/test.json");
        EXPECT_EQ(scheme, "file");
        EXPECT_EQ(auth, "");
        EXPECT_EQ(token, "");
#endif
    }

    TEST(path, is_path)
    {
        EXPECT_TRUE(is_path("./"));
        EXPECT_TRUE(is_path(".."));
        EXPECT_TRUE(is_path("~"));
        EXPECT_TRUE(is_path("/"));
        EXPECT_FALSE(is_path("file://makefile"));
    }
}  // namespace mamba
