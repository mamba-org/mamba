#include <gtest/gtest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/channel_internal.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"

#include "mamba/api/install.hpp"

namespace mamba
{
    std::string fix_win_path(const std::string& path);

#ifdef __linux__
    std::string platform("linux-64");
#elif __APPLE__ && __x86_64__
    std::string platform("osx-64");
#elif __APPLE__ && __arm64__
    std::string platform("osx-arm64");
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
        EXPECT_EQ(ch.name(), "<alias>");
        EXPECT_EQ(ch.canonical_name(), "<alias>");

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

    TEST(ChannelContext, channel_alias)
    {
        // ChannelContext builds its custom channels with
        // make_simple_channel
        auto& ctx = Context::instance();
        ctx.channel_alias = "https://mydomain.com/channels/";
        ChannelContext::instance().reset();

        const auto& ch = ChannelContext::instance().get_channel_alias();
        EXPECT_EQ(ch.scheme(), "https");
        EXPECT_EQ(ch.location(), "mydomain.com/channels");
        EXPECT_EQ(ch.name(), "<alias>");
        EXPECT_EQ(ch.canonical_name(), "<alias>");

        const auto& custom = ChannelContext::instance().get_custom_channels();

        auto it = custom.find("pkgs/main");
        EXPECT_NE(it, custom.end());
        EXPECT_EQ(it->second.name(), "pkgs/main");
        EXPECT_EQ(it->second.location(), "repo.anaconda.com");
        EXPECT_EQ(it->second.canonical_name(), "defaults");

        std::string value = "conda-forge";
        const Channel& c = make_channel(value);
        EXPECT_EQ(c.scheme(), "https");
        EXPECT_EQ(c.location(), "mydomain.com/channels");
        EXPECT_EQ(c.name(), "conda-forge");
        EXPECT_EQ(c.canonical_name(), "conda-forge");
        // EXPECT_EQ(c.url(), "conda-forge");
        EXPECT_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));

        ctx.channel_alias = "https://conda.anaconda.org";
        ChannelContext::instance().reset();
    }

    TEST(ChannelContext, custom_channels)
    {
        // ChannelContext builds its custom channels with
        // make_simple_channel
        auto& ctx = Context::instance();
        ctx.channel_alias = "https://mydomain.com/channels/";
        ctx.custom_channels = {
            { "test_channel", "file:///tmp" },
            { "some_channel", "https://conda.mydomain.xyz/" },
        };
        ChannelContext::instance().reset();

        const auto& ch = ChannelContext::instance().get_channel_alias();
        EXPECT_EQ(ch.scheme(), "https");
        EXPECT_EQ(ch.location(), "mydomain.com/channels");
        EXPECT_EQ(ch.name(), "<alias>");
        EXPECT_EQ(ch.canonical_name(), "<alias>");

        {
            std::string value = "test_channel";
            const Channel& c = make_channel(value);
            EXPECT_EQ(c.scheme(), "file");
            EXPECT_EQ(c.location(), "");
            EXPECT_EQ(c.name(), "tmp/test_channel");
            EXPECT_EQ(c.canonical_name(), "test_channel");
            EXPECT_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
            std::vector<std::string> exp_urls({ std::string("file:///tmp/test_channel/") + platform,
                                                std::string("file:///tmp/test_channel/noarch") });
            EXPECT_EQ(c.urls(), exp_urls);
        }

        {
            std::string value = "some_channel";
            const Channel& c = make_channel(value);
            EXPECT_EQ(c.scheme(), "https");
            EXPECT_EQ(c.location(), "conda.mydomain.xyz");
            EXPECT_EQ(c.name(), "some_channel");
            EXPECT_EQ(c.canonical_name(), "some_channel");
            EXPECT_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
            std::vector<std::string> exp_urls(
                { std::string("https://conda.mydomain.xyz/some_channel/") + platform,
                  std::string("https://conda.mydomain.xyz/some_channel/noarch") });
            EXPECT_EQ(c.urls(), exp_urls);
        }

        ctx.channel_alias = "https://conda.anaconda.org";
        ctx.custom_channels.clear();
        ChannelContext::instance().reset();
    }

    TEST(ChannelContext, custom_channels_with_labels)
    {
        // ChannelContext builds its custom channels with
        // make_simple_channel
        auto& ctx = Context::instance();
        ctx.custom_channels = {
            { "test_channel", "https://server.com/private/channels" },
        };
        ChannelContext::instance().reset();

        // const auto& ch = ChannelContext::instance().get_channel_alias();
        // EXPECT_EQ(ch.scheme(), "https");
        // EXPECT_EQ(ch.location(), "mydomain.com/channels");
        // EXPECT_EQ(ch.name(), "<alias>");
        // EXPECT_EQ(ch.canonical_name(), "<alias>");

        {
            std::string value = "test_channel";
            const Channel& c = make_channel(value);
            EXPECT_EQ(c.scheme(), "https");
            EXPECT_EQ(c.location(), "server.com");
            EXPECT_EQ(c.name(), "private/channels/test_channel");
            EXPECT_EQ(c.canonical_name(), "test_channel");
            EXPECT_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
            std::vector<std::string> exp_urls(
                { std::string("https://server.com/private/channels/test_channel/") + platform,
                  std::string("https://server.com/private/channels/test_channel/noarch") });
            EXPECT_EQ(c.urls(), exp_urls);
        }

        {
            std::string value = "test_channel/mylabel/xyz";
            const Channel& c = make_channel(value);
            EXPECT_EQ(c.scheme(), "https");
            EXPECT_EQ(c.location(), "server.com");
            EXPECT_EQ(c.name(), "private/channels/test_channel/mylabel/xyz");
            EXPECT_EQ(c.canonical_name(), "test_channel/mylabel/xyz");
            EXPECT_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
            std::vector<std::string> exp_urls(
                { std::string("https://server.com/private/channels/test_channel/mylabel/xyz/")
                      + platform,
                  std::string(
                      "https://server.com/private/channels/test_channel/mylabel/xyz/noarch") });
            EXPECT_EQ(c.urls(), exp_urls);
        }

        ctx.channel_alias = "https://conda.anaconda.org";
        ctx.custom_channels.clear();
        ChannelContext::instance().reset();
    }


    TEST(Channel, make_channel)
    {
        std::string value = "conda-forge";
        const Channel& c = make_channel(value);
        EXPECT_EQ(c.scheme(), "https");
        EXPECT_EQ(c.location(), "conda.anaconda.org");
        EXPECT_EQ(c.name(), "conda-forge");
        EXPECT_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));

        std::string value2 = "https://repo.anaconda.com/pkgs/main[" + platform + "]";
        const Channel& c2 = make_channel(value2);
        EXPECT_EQ(c2.scheme(), "https");
        EXPECT_EQ(c2.location(), "repo.anaconda.com");
        EXPECT_EQ(c2.name(), "pkgs/main");
        EXPECT_EQ(c2.platforms(), std::vector<std::string>({ platform }));

        std::string value3 = "https://conda.anaconda.org/conda-forge[" + platform + "]";
        const Channel& c3 = make_channel(value3);
        EXPECT_EQ(c3.scheme(), c.scheme());
        EXPECT_EQ(c3.location(), c.location());
        EXPECT_EQ(c3.name(), c.name());
        EXPECT_EQ(c3.platforms(), std::vector<std::string>({ platform }));

        std::string value4 = "/home/mamba/test/channel_b";
        const Channel& c4 = make_channel(value4);
        EXPECT_EQ(c4.scheme(), "file");
#ifdef _WIN32
        std::string driveletter = fs::absolute(fs::path("/")).string().substr(0, 1);
        EXPECT_EQ(c4.location(), driveletter + ":/home/mamba/test");
#else
        EXPECT_EQ(c4.location(), "/home/mamba/test");
#endif
        EXPECT_EQ(c4.name(), "channel_b");
        EXPECT_EQ(c4.platforms(), std::vector<std::string>({ platform, "noarch" }));

        std::string value5 = "/home/mamba/test/channel_b[" + platform + "]";
        const Channel& c5 = make_channel(value5);
        EXPECT_EQ(c5.scheme(), "file");
#ifdef _WIN32
        EXPECT_EQ(c5.location(), driveletter + ":/home/mamba/test");
#else
        EXPECT_EQ(c5.location(), "/home/mamba/test");
#endif
        EXPECT_EQ(c5.name(), "channel_b");
        EXPECT_EQ(c5.platforms(), std::vector<std::string>({ platform }));

        std::string value6a = "http://localhost:8000/conda-forge[noarch]";
        const Channel& c6a = make_channel(value6a);
        EXPECT_EQ(c6a.urls(false),
                  std::vector<std::string>({ "http://localhost:8000/conda-forge/noarch" }));

        std::string value6b = "http://localhost:8000/conda_mirror/conda-forge[noarch]";
        const Channel& c6b = make_channel(value6b);
        EXPECT_EQ(
            c6b.urls(false),
            std::vector<std::string>({ "http://localhost:8000/conda_mirror/conda-forge/noarch" }));

        std::string value7 = "conda-forge[noarch,arbitrary]";
        const Channel& c7 = make_channel(value7);
        EXPECT_EQ(c7.platforms(), std::vector<std::string>({ "noarch", "arbitrary" }));
    }

    TEST(Channel, urls)
    {
        std::string value = "https://conda.anaconda.org/conda-forge[noarch,win-64,arbitrary]";
        const Channel& c = make_channel(value);
        EXPECT_EQ(c.urls(),
                  std::vector<std::string>({ "https://conda.anaconda.org/conda-forge/noarch",
                                             "https://conda.anaconda.org/conda-forge/win-64",
                                             "https://conda.anaconda.org/conda-forge/arbitrary" }));

        const Channel& c1 = make_channel("https://conda.anaconda.org/conda-forge");
        EXPECT_EQ(c1.urls(),
                  std::vector<std::string>({ "https://conda.anaconda.org/conda-forge/" + platform,
                                             "https://conda.anaconda.org/conda-forge/noarch" }));
    }

    TEST(Channel, add_token)
    {
        auto& ctx = Context::instance();
        ctx.channel_tokens["https://conda.anaconda.org"] = "my-12345-token";

        ChannelInternal::clear_cache();

        const auto& chan = make_channel("conda-forge[noarch]");
        EXPECT_EQ(chan.token(), "my-12345-token");
        EXPECT_EQ(chan.urls(true),
                  std::vector<std::string>{
                      { "https://conda.anaconda.org/t/my-12345-token/conda-forge/noarch" } });
        EXPECT_EQ(chan.urls(false),
                  std::vector<std::string>{ { "https://conda.anaconda.org/conda-forge/noarch" } });
    }

    TEST(Channel, fix_win_file_path)
    {
        if (platform == "win-64")
        {
            const Channel& c = make_channel("C:\\test\\channel");
            EXPECT_EQ(c.urls(false),
                      std::vector<std::string>(
                          { "file:///C:/test/channel/win-64", "file:///C:/test/channel/noarch" }));
        }
        else
        {
            const Channel& c = make_channel("/test/channel");
            EXPECT_EQ(c.urls(false),
                      std::vector<std::string>({ std::string("file:///test/channel/") + platform,
                                                 "file:///test/channel/noarch" }));
        }
    }

    TEST(Channel, trailing_slash)
    {
        const Channel& c = make_channel("http://localhost:8000/");
        EXPECT_EQ(c.platform_url("win-64", false), "http://localhost:8000/win-64");
        EXPECT_EQ(c.base_url(), "http://localhost:8000");
        std::vector<std::string> expected_urls(
            { std::string("http://localhost:8000/") + platform, "http://localhost:8000/noarch" });
        EXPECT_EQ(c.urls(true), expected_urls);
        const Channel& c4 = make_channel("http://localhost:8000");
        EXPECT_EQ(c4.platform_url("linux-64", false), "http://localhost:8000/linux-64");
        const Channel& c2 = make_channel("http://user:test@localhost:8000/");
        EXPECT_EQ(c2.platform_url("win-64", false), "http://localhost:8000/win-64");
        EXPECT_EQ(c2.platform_url("win-64", true), "http://user:test@localhost:8000/win-64");
        const Channel& c3
            = make_channel("https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012");
        EXPECT_EQ(c3.platform_url("win-64", false), "https://localhost:8000/win-64");
        EXPECT_EQ(c3.platform_url("win-64", true),
                  "https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012/win-64");

        std::vector<std::string> expected_urls2(
            { std::string("https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012/")
                  + platform,
              "https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012/noarch" });

        EXPECT_EQ(c3.urls(true), expected_urls2);
    }

    TEST(Channel, load_tokens)
    {
        // touch(env::home_directory() / ".continuum" / "anaconda")
        // auto& ctx = Context::instance();
        // ctx.channel_tokens["https://conda.anaconda.org"] = "my-12345-token";

        // Channel::clear_cache();

        // const auto& chan = make_channel("conda-forge");
        // EXPECT_EQ(chan.token(), "my-12345-token");
        // EXPECT_EQ(chan.url(true),
        // "https://conda.anaconda.org/t/my-12345-token/conda-forge/noarch");
        // EXPECT_EQ(chan.url(false), "https://conda.anaconda.org/conda-forge/noarch");
    }
}  // namespace mamba
