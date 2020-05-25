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
        std::string value = "conda-forge";
        Channel& c = make_channel(value);
        EXPECT_EQ(c.scheme(), "https");
        EXPECT_EQ(c.location(), "conda.anaconda.org");
        EXPECT_EQ(c.name(), "conda-forge");
        EXPECT_EQ(c.platform(), "");

        std::string value2 = "https://repo.anaconda.com/pkgs/main/linux-64";
        Channel& c2 = make_channel(value2);
        EXPECT_EQ(c2.scheme(), "https");
        EXPECT_EQ(c2.location(), "repo.anaconda.com");
        EXPECT_EQ(c2.name(), "pkgs/main");
        EXPECT_EQ(c2.platform(), "linux-64");

        std::string value3 = "https://conda.anaconda.org/conda-forge/linux-64";
        Channel& c3 = make_channel(value3);
        EXPECT_EQ(c3.scheme(), c.scheme());
        EXPECT_EQ(c3.location(), c.location());
        EXPECT_EQ(c3.name(), c.name());
        EXPECT_EQ(c3.platform(), "linux-64");

        std::string value4 = "/home/mamba/test/channel_b";
        Channel& c4 = make_channel(value4);
        EXPECT_EQ(c4.scheme(), "file");
        EXPECT_EQ(c4.location(), "/home/mamba/test");
        EXPECT_EQ(c4.name(), "channel_b");
        EXPECT_EQ(c4.platform(), "");

        std::string value5 = "/home/mamba/test/channel_b/linux-64";
        Channel& c5 = make_channel(value5);
        EXPECT_EQ(c5.scheme(), "file");
        EXPECT_EQ(c5.location(), "/home/mamba/test");
        EXPECT_EQ(c5.name(), "channel_b");
        EXPECT_EQ(c5.platform(), "linux-64");
    }

    TEST(Channel, urls)
    {
        std::string value = "https://conda.anaconda.org/conda-forge/linux-64";
        std::vector<std::string> platforms = { "win-64", "noarch" };

        Channel& c = make_channel(value);
        std::vector<std::string> urls = c.urls(platforms);
        EXPECT_EQ(urls[0], value);
        EXPECT_EQ(urls[1], "https://conda.anaconda.org/conda-forge/noarch");

        std::vector<std::string> urls10 = c.urls();
        EXPECT_EQ(urls[0], urls10[0]);
        EXPECT_EQ(urls[1], urls10[1]);

        Channel& c1 = make_channel("https://conda.anaconda.org/conda-forge");
        std::vector<std::string> urls2 = c1.urls(platforms);
        EXPECT_EQ(urls2[0], "https://conda.anaconda.org/conda-forge/win-64");
        EXPECT_EQ(urls2[1], "https://conda.anaconda.org/conda-forge/noarch");
    }

    TEST(Channel, calculate_channel_urls)
    {
        std::vector<std::string> urls = { "conda-forge", "defaults" };
        std::vector<std::string> res = calculate_channel_urls(urls, true);
        EXPECT_EQ(res.size(), 6);
        EXPECT_EQ(res[0], "https://conda.anaconda.org/conda-forge/linux-64");
        EXPECT_EQ(res[1], "https://conda.anaconda.org/conda-forge/noarch");
        EXPECT_EQ(res[2], "https://repo.anaconda.com/pkgs/main/linux-64");
        EXPECT_EQ(res[3], "https://repo.anaconda.com/pkgs/main/noarch");
        EXPECT_EQ(res[4], "https://repo.anaconda.com/pkgs/r/linux-64");
        EXPECT_EQ(res[5], "https://repo.anaconda.com/pkgs/r/noarch");

        std::vector<std::string> res2 = calculate_channel_urls(urls, false);
        EXPECT_EQ(res2.size(), 6);
        EXPECT_EQ(res2[0], res[0]);
        EXPECT_EQ(res2[1], res[1]);
        EXPECT_EQ(res2[2], res[2]);
        EXPECT_EQ(res2[3], res[3]);
        EXPECT_EQ(res2[4], res[4]);
        EXPECT_EQ(res2[5], res[5]);
    }
}

