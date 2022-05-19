#include <gtest/gtest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/channel_builder.hpp"
#include "mamba/core/output.hpp"

namespace mamba
{
    std::string fix_win_path(const std::string& path);

    void split_platform(const std::vector<std::string>& known_platforms,
                        const std::string& url,
                        std::string& cleaned_url,
                        std::string& platform);

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

    // Regression test for https://github.com/mamba-org/mamba/issues/1671
    TEST(ChannelContext, channel_alias_with_custom_default_channels)
    {
        auto& ctx = Context::instance();
        auto old_default_channels = ctx.default_channels;
        ctx.channel_alias = "https://ali.as/";
        ctx.default_channels = { "prefix" };
        ctx.channels = { "prefix-and-more" };
        ChannelContext::instance().reset();

        auto base = std::string("https://ali.as/prefix-and-more/");
        auto& chan = make_channel(base);
        std::vector<std::string> expected_urls = { base + platform, base + "noarch" };
        EXPECT_EQ(chan.urls(), expected_urls);

        ctx.channel_alias = "https://conda.anaconda.org";
        ctx.custom_channels.clear();
        ctx.default_channels = old_default_channels;
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

    TEST(ChannelContext, custom_multichannels)
    {
        // ChannelContext builds its custom channels with
        // make_simple_channel
        auto& ctx = Context::instance();
        ctx.custom_multichannels["xtest"]
            = std::vector<std::string>{ "https://mydomain.com/conda-forge",
                                        "https://mydomain.com/bioconda",
                                        "https://mydomain.com/snakepit" };
        ctx.custom_multichannels["ytest"]
            = std::vector<std::string>{ "https://otherdomain.com/conda-forge",
                                        "https://otherdomain.com/bioconda",
                                        "https://otherdomain.com/snakepit" };

        ChannelContext::instance().reset();

        auto x = get_channels({ "xtest" });

        EXPECT_EQ(x.size(), 3);
        auto* c1 = x[0];

        std::vector<std::string> exp_urls(
            { std::string("https://mydomain.com/conda-forge/") + platform,
              std::string("https://mydomain.com/conda-forge/noarch") });

        EXPECT_EQ(c1->urls(), exp_urls);

        std::vector<std::string> exp_urlsy3(
            { std::string("https://otherdomain.com/snakepit/") + platform,
              std::string("https://otherdomain.com/snakepit/noarch") });

        auto y = get_channels({ "ytest" });
        auto* y3 = y[2];

        EXPECT_EQ(y3->urls(), exp_urlsy3);

        ctx.channel_alias = "https://conda.anaconda.org";
        ctx.custom_multichannels.clear();
        ChannelContext::instance().reset();
    }

    TEST(ChannelContext, custom_extended_multichannels)
    {
        // ChannelContext builds its custom channels with
        // make_simple_channel
        auto& ctx = Context::instance();

        ctx.channel_alias = "https://condaforge.org/channels/";

        ctx.custom_channels["xyz"] = "https://mydomain.xyz/xyzchannel";

        ctx.custom_multichannels["everything"]
            = std::vector<std::string>{ "conda-forge", "https://mydomain.com/bioconda", "xyz" };

        ChannelContext::instance().reset();

        auto x = get_channels({ "everything" });

        EXPECT_EQ(x.size(), 3);
        auto* c1 = x[0];
        auto* c2 = x[1];
        auto* c3 = x[2];

        std::vector<std::string> exp_urls(
            { std::string("https://condaforge.org/channels/conda-forge/") + platform,
              std::string("https://condaforge.org/channels/conda-forge/noarch") });

        EXPECT_EQ(c1->urls(), exp_urls);

        std::vector<std::string> exp_urls2(
            { std::string("https://mydomain.com/bioconda/") + platform,
              std::string("https://mydomain.com/bioconda/noarch") });

        EXPECT_EQ(c2->urls(), exp_urls2);

        std::vector<std::string> exp_urls3(
            { std::string("https://mydomain.xyz/xyzchannel/xyz/") + platform,
              std::string("https://mydomain.xyz/xyzchannel/xyz/noarch") });

        EXPECT_EQ(c3->urls(), exp_urls3);

        ctx.channel_alias = "https://conda.anaconda.org";
        ctx.custom_multichannels.clear();
        ctx.custom_channels.clear();
        ChannelContext::instance().reset();
    }


    TEST(ChannelContext, default_channels)
    {
        auto& ctx = Context::instance();
        ChannelContext::instance().reset();

        auto x = get_channels({ "defaults" });
#if !defined(_WIN32)
        const Channel* c1 = x[0];
        const Channel* c2 = x[1];

        EXPECT_EQ(c1->name(), "pkgs/main");
        std::vector<std::string> exp_urls(
            { std::string("https://repo.anaconda.com/pkgs/main/") + platform,
              std::string("https://repo.anaconda.com/pkgs/main/noarch") });
        EXPECT_EQ(c1->urls(), exp_urls);

        EXPECT_EQ(c2->name(), "pkgs/r");
        std::vector<std::string> exp_urls2(
            { std::string("https://repo.anaconda.com/pkgs/r/") + platform,
              std::string("https://repo.anaconda.com/pkgs/r/noarch") });
        EXPECT_EQ(c2->urls(), exp_urls2);

        EXPECT_EQ(c1->location(), "repo.anaconda.com");
        EXPECT_EQ(c1->scheme(), "https");

#endif
        ctx.custom_channels.clear();
        ChannelContext::instance().reset();
    }

    TEST(ChannelContext, custom_default_channels)
    {
        auto& ctx = Context::instance();
        ctx.default_channels
            = { "https://mamba.com/test/channel", "https://mamba.com/stable/channel" };
        ChannelContext::instance().reset();

        auto x = get_channels({ "defaults" });
        const Channel* c1 = x[0];
        const Channel* c2 = x[1];

        EXPECT_EQ(c1->name(), "test/channel");
        std::vector<std::string> exp_urls(
            { std::string("https://mamba.com/test/channel/") + platform,
              std::string("https://mamba.com/test/channel/noarch") });
        EXPECT_EQ(c1->urls(), exp_urls);
        std::vector<std::string> exp_urls2(
            { std::string("https://mamba.com/stable/channel/") + platform,
              std::string("https://mamba.com/stable/channel/noarch") });
        EXPECT_EQ(c2->urls(), exp_urls2);

        EXPECT_EQ(c2->name(), "stable/channel");
        EXPECT_EQ(c2->location(), "mamba.com");
        EXPECT_EQ(c2->scheme(), "https");

        ctx.custom_channels.clear();
        ChannelContext::instance().reset();
    }

    TEST(ChannelContext, custom_channels_with_labels)
    {
        auto& ctx = Context::instance();
        ctx.custom_channels = {
            { "test_channel", "https://server.com/private/channels" },
        };
        ChannelContext::instance().reset();

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

    TEST(Channel, channel_name)
    {
        std::string value = "https://repo.mamba.pm/conda-forge";
        const Channel& c = make_channel(value);
        EXPECT_EQ(c.scheme(), "https");
        EXPECT_EQ(c.location(), "repo.mamba.pm");
        EXPECT_EQ(c.name(), "conda-forge");
        EXPECT_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
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
        ctx.authentication_info()["https://conda.anaconda.org"]
            = AuthenticationInfo{ AuthenticationType::kCondaToken, "my-12345-token" };

        ChannelBuilder::clear_cache();

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

    TEST(Channel, split_platform)
    {
        std::string platform, cleaned_url;
        split_platform({ "noarch", "linux-64" },
                       "https://mamba.com/linux-64/package.tar.bz2",
                       cleaned_url,
                       platform);

        EXPECT_EQ(platform, "linux-64");
        EXPECT_EQ(cleaned_url, "https://mamba.com/package.tar.bz2");

        split_platform({ "noarch", "linux-64" },
                       "https://mamba.com/linux-64/noarch-package.tar.bz2",
                       cleaned_url,
                       platform);
        EXPECT_EQ(platform, "linux-64");
        EXPECT_EQ(cleaned_url, "https://mamba.com/noarch-package.tar.bz2");

        split_platform({ "linux-64", "osx-arm64", "noarch" },
                       "https://mamba.com/noarch/kernel_linux-64-package.tar.bz2",
                       cleaned_url,
                       platform);
        EXPECT_EQ(platform, "noarch");
        EXPECT_EQ(cleaned_url, "https://mamba.com/kernel_linux-64-package.tar.bz2");

        split_platform(
            { "noarch", "linux-64" }, "https://mamba.com/linux-64", cleaned_url, platform);

        EXPECT_EQ(platform, "linux-64");
        EXPECT_EQ(cleaned_url, "https://mamba.com");

        split_platform({ "noarch", "linux-64" }, "https://mamba.com/noarch", cleaned_url, platform);

        EXPECT_EQ(platform, "noarch");
        EXPECT_EQ(cleaned_url, "https://mamba.com");
    }
}  // namespace mamba
