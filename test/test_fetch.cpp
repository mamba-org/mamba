#include <gtest/gtest.h>

#include "mamba/fetch.hpp"

namespace mamba
{
    std::string unc_url(const std::string& path);

    TEST(DownloadTarget, unc_url)
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
}
