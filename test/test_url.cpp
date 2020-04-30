#include <gtest/gtest.h>

#include "url.hpp"

namespace mamba
{
    TEST(url, parse)
    {
        {
            URLParser m("http://mamba.org");
            EXPECT_EQ(m.scheme(), "http");
            EXPECT_EQ(m.path(), "/");
            EXPECT_EQ(m.host(), "mamba.org");
        }
        {
            URLParser m("s3://userx123:üúßsajd@mamba.org");
            EXPECT_EQ(m.scheme(), "s3");
            EXPECT_EQ(m.path(), "/");
            EXPECT_EQ(m.host(), "mamba.org");
            EXPECT_EQ(m.user(), "userx123");
            EXPECT_EQ(m.password(), "üúßsajd");
        }
        {
            URLParser m("https://mamba🆒🔬.org/this/is/a/path/?query=123&xyz=3333");
            EXPECT_EQ(m.scheme(), "https");
            EXPECT_EQ(m.path(), "/this/is/a/path/");
            EXPECT_EQ(m.host(), "mamba🆒🔬.org");
            EXPECT_EQ(m.query(), "query=123&xyz=3333");
        }
    }
}