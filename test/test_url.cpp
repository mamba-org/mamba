#include <gtest/gtest.h>

#include "url.hpp"

namespace mamba
{
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
    }
}
