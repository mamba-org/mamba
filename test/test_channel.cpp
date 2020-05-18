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
    }
}

