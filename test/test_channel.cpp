#include <gtest/gtest.h>

#include "mamba/core/channel.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
    std::string fix_win_path(const std::string& path);

#ifdef __linux__
    std::string platform("linux-64");
#elif __APPLE__
    std::string platform("osx-64");
#elif _WIN32
    std::string platform("win-64");
#endif

#ifdef _WIN32
    TEST(Channel, fix_win_path)
    {
        std::string test_str("file://\\unc\\path\\on\\win");
        auto out = fix_win_path(test_str);
        EXPECT_EQ(out, "file:///unc/path/on/win");
        auto out2 = fix_win_path("file://C:\\Program\\ (x74)\\Users\\hello\\ world");
        EXPECT_EQ(out2, "file://C:/Program\\ (x74)/Users/hello\\ world");
        auto out3 = fix_win_path("file://\\\\Programs\\xyz");
        EXPECT_EQ(out3, "file://Programs/xyz");
    }
#endif

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

        std::string value2 = "https://repo.anaconda.com/pkgs/main/" + platform;
        Channel& c2 = make_channel(value2);
        EXPECT_EQ(c2.scheme(), "https");
        EXPECT_EQ(c2.location(), "repo.anaconda.com");
        EXPECT_EQ(c2.name(), "pkgs/main");
        EXPECT_EQ(c2.platform(), platform);

        std::string value3 = "https://conda.anaconda.org/conda-forge/" + platform;
        Channel& c3 = make_channel(value3);
        EXPECT_EQ(c3.scheme(), c.scheme());
        EXPECT_EQ(c3.location(), c.location());
        EXPECT_EQ(c3.name(), c.name());
        EXPECT_EQ(c3.platform(), platform);

        std::string value4 = "/home/mamba/test/channel_b";
        Channel& c4 = make_channel(value4);
        EXPECT_EQ(c4.scheme(), "file");
#ifdef _WIN32
        std::string driveletter = fs::absolute(fs::path("/")).string().substr(0, 1);
        EXPECT_EQ(c4.location(), driveletter + ":/home/mamba/test");
#else
        EXPECT_EQ(c4.location(), "/home/mamba/test");
#endif
        EXPECT_EQ(c4.name(), "channel_b");
        EXPECT_EQ(c4.platform(), "");

        std::string value5 = "/home/mamba/test/channel_b/" + platform;
        Channel& c5 = make_channel(value5);
        EXPECT_EQ(c5.scheme(), "file");
#ifdef _WIN32
        EXPECT_EQ(c5.location(), driveletter + ":/home/mamba/test");
#else
        EXPECT_EQ(c5.location(), "/home/mamba/test");
#endif
        EXPECT_EQ(c5.name(), "channel_b");
        EXPECT_EQ(c5.platform(), platform);

        std::string value6a = "http://localhost:8000/conda-forge/noarch";
        Channel& c6a = make_channel(value6a);
        EXPECT_EQ(c6a.url(false), value6a);

        std::string value6b = "http://localhost:8000/conda_mirror/conda-forge/noarch";
        Channel& c6b = make_channel(value6b);
        EXPECT_EQ(c6b.url(false), value6b);
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
        EXPECT_EQ(res.size(), on_win ? 8u : 6u);
        EXPECT_EQ(res[0], "https://conda.anaconda.org/conda-forge/" + platform);
        EXPECT_EQ(res[1], "https://conda.anaconda.org/conda-forge/noarch");
        EXPECT_EQ(res[2], "https://repo.anaconda.com/pkgs/main/" + platform);
        EXPECT_EQ(res[3], "https://repo.anaconda.com/pkgs/main/noarch");
        EXPECT_EQ(res[4], "https://repo.anaconda.com/pkgs/r/" + platform);
        EXPECT_EQ(res[5], "https://repo.anaconda.com/pkgs/r/noarch");

        std::vector<std::string> res2 = calculate_channel_urls(urls, false);
        EXPECT_EQ(res2.size(), on_win ? 8u : 6u);
        EXPECT_EQ(res2[0], res[0]);
        EXPECT_EQ(res2[1], res[1]);
        EXPECT_EQ(res2[2], res[2]);
        EXPECT_EQ(res2[3], res[3]);
        EXPECT_EQ(res2[4], res[4]);
        EXPECT_EQ(res2[5], res[5]);

#ifdef _WIN32
        EXPECT_EQ(res[6], "https://repo.anaconda.com/pkgs/msys2/" + platform);
        EXPECT_EQ(res[7], "https://repo.anaconda.com/pkgs/msys2/noarch");
        EXPECT_EQ(res2[6], res[6]);
        EXPECT_EQ(res2[7], res[7]);
#endif

        std::vector<std::string> local_urls = { "./channel_b", "./channel_a" };
        std::vector<std::string> local_res = calculate_channel_urls(local_urls, false);
        std::string current_dir = path_to_url(fs::current_path().string()) + '/';
        EXPECT_EQ(local_res.size(), 4u);
        EXPECT_EQ(local_res[0], current_dir + "channel_b/" + platform);
        EXPECT_EQ(local_res[1], current_dir + "channel_b/noarch");
        EXPECT_EQ(local_res[2], current_dir + "channel_a/" + platform);
        EXPECT_EQ(local_res[3], current_dir + "channel_a/noarch");
    }
}  // namespace mamba
