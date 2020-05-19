#include <gtest/gtest.h>

#include "channel.hpp"

namespace mamba
{
    TEST(ChannelContext, init)
    {
        // ChannelContext builds its custom channels with
        // make_simple_channel
        
        const auto& ch = ChannelContext::instance().get_channel_alias();
        EXPECT_EQ(ch.scheme(), "https");
        EXPECT_EQ(ch.location(), "conda.anaconda.org");
        EXPECT_EQ(ch.name(), "");
        EXPECT_EQ(ch.canonical_name(), "");

        const auto& custom = ChannelContext::instance().get_custom_channels();
        
        auto it = custom.find("pkgs/main");
        EXPECT_NE(it, custom.end());
        EXPECT_EQ(it->second.name(), "pkgs/main");
        EXPECT_EQ(it->second.location(), "repo.anaconda.com");
        EXPECT_EQ(it->second.canonical_name(), "defaults");

        it = custom.find("pkgs/pro");
        EXPECT_NE(it, custom.end());
        EXPECT_EQ(it->second.name(), "pkgs/pro");
        EXPECT_EQ(it->second.location(), "repo.anaconda.com");
        EXPECT_EQ(it->second.canonical_name(), "pkgs/pro");
        
        it = custom.find("pkgs/r");
        EXPECT_NE(it, custom.end());
        EXPECT_EQ(it->second.name(), "pkgs/r");
        EXPECT_EQ(it->second.location(), "repo.anaconda.com");
        EXPECT_EQ(it->second.canonical_name(), "defaults");
    }

    TEST(Channel, make_channel)
    {
        std::string value = "https://conda.anaconda.org/conda-forge/linux-64";
        Channel& c = make_channel(value);
        EXPECT_EQ(c.scheme(), "https");
        EXPECT_EQ(c.location(), "conda.anaconda.org");
        EXPECT_EQ(c.name(), "conda-forge");
        EXPECT_EQ(c.platform(), "linux-64");

        std::string value2 = "https://repo.anaconda.com/pkgs/main/linux-64";
        Channel& c2 = make_channel(value2);
        EXPECT_EQ(c2.scheme(), "https");
        EXPECT_EQ(c2.location(), "repo.anaconda.com");
        EXPECT_EQ(c2.name(), "pkgs/main");
        EXPECT_EQ(c2.platform(), "linux-64");
    }

    TEST(Channel, urls)
    {
        std::string value = "https://conda.anaconda.org/conda-forge/linux-64";
        std::vector<std::string> platforms = { "win-64", "noarch" };

        Channel& c = make_channel(value);
        std::vector<std::string> urls = c.urls(platforms);
        EXPECT_EQ(urls[0], value);
        EXPECT_EQ(urls[1], "https://conda.anaconda.org/conda-forge/noarch");

        Channel& c1 = make_channel("https://conda.anaconda.org/conda-forge");
        std::vector<std::string> urls2 = c1.urls(platforms);
        EXPECT_EQ(urls2[0], "https://conda.anaconda.org/conda-forge/win-64");
        EXPECT_EQ(urls2[1], "https://conda.anaconda.org/conda-forge/noarch");
    }
}

