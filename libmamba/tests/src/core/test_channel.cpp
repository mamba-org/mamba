#include <type_traits>

#include <doctest/doctest.h>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"

namespace mamba
{
    std::string fix_win_path(const std::string& path);

    void split_platform(
        const std::vector<std::string>& known_platforms,
        const std::string& url,
        std::string& cleaned_url,
        std::string& platform
    );

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
    TEST_SUITE("Channel")
    {
        TEST_CASE("fix_win_path")
        {
            std::string test_str("file://\\unc\\path\\on\\win");
            auto out = fix_win_path(test_str);
            CHECK_EQ(out, "file:///unc/path/on/win");
            auto out2 = fix_win_path("file://C:\\Program\\ (x74)\\Users\\hello\\ world");
            CHECK_EQ(out2, "file://C:/Program\\ (x74)/Users/hello\\ world");
            auto out3 = fix_win_path("file://\\\\Programs\\xyz");
            CHECK_EQ(out3, "file://Programs/xyz");
        }
    }
#endif

    static_assert(std::is_move_constructible_v<mamba::Channel>);
    static_assert(std::is_move_assignable_v<mamba::Channel>);

    TEST_SUITE("ChannelContext")
    {
        TEST_CASE("init")
        {
            // ChannelContext builds its custom channels with
            // make_simple_channel
            ChannelContext channel_context;
            const auto& ch = channel_context.get_channel_alias();
            CHECK_EQ(ch.scheme(), "https");
            CHECK_EQ(ch.location(), "conda.anaconda.org");
            CHECK_EQ(ch.name(), "<alias>");
            CHECK_EQ(ch.canonical_name(), "<alias>");

            const auto& custom = channel_context.get_custom_channels();

            auto it = custom.find("pkgs/main");
            CHECK_NE(it, custom.end());
            CHECK_EQ(it->second.name(), "pkgs/main");
            CHECK_EQ(it->second.location(), "repo.anaconda.com");
            CHECK_EQ(it->second.canonical_name(), "defaults");

            it = custom.find("pkgs/pro");
            CHECK_NE(it, custom.end());
            CHECK_EQ(it->second.name(), "pkgs/pro");
            CHECK_EQ(it->second.location(), "repo.anaconda.com");
            CHECK_EQ(it->second.canonical_name(), "pkgs/pro");

            it = custom.find("pkgs/r");
            CHECK_NE(it, custom.end());
            CHECK_EQ(it->second.name(), "pkgs/r");
            CHECK_EQ(it->second.location(), "repo.anaconda.com");
            CHECK_EQ(it->second.canonical_name(), "defaults");
        }

        TEST_CASE("channel_alias")
        {
            // ChannelContext builds its custom channels with
            // make_simple_channel
            auto& ctx = Context::instance();
            ctx.channel_alias = "https://mydomain.com/channels/";

            ChannelContext channel_context;

            const auto& ch = channel_context.get_channel_alias();
            CHECK_EQ(ch.scheme(), "https");
            CHECK_EQ(ch.location(), "mydomain.com/channels");
            CHECK_EQ(ch.name(), "<alias>");
            CHECK_EQ(ch.canonical_name(), "<alias>");

            const auto& custom = channel_context.get_custom_channels();

            auto it = custom.find("pkgs/main");
            CHECK_NE(it, custom.end());
            CHECK_EQ(it->second.name(), "pkgs/main");
            CHECK_EQ(it->second.location(), "repo.anaconda.com");
            CHECK_EQ(it->second.canonical_name(), "defaults");

            std::string value = "conda-forge";
            const Channel& c = channel_context.make_channel(value);
            CHECK_EQ(c.scheme(), "https");
            CHECK_EQ(c.location(), "mydomain.com/channels");
            CHECK_EQ(c.name(), "conda-forge");
            CHECK_EQ(c.canonical_name(), "conda-forge");
            // CHECK_EQ(c.url(), "conda-forge");
            CHECK_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));

            ctx.channel_alias = "https://conda.anaconda.org";
        }

        // Regression test for https://github.com/mamba-org/mamba/issues/1671
        TEST_CASE("channel_alias_with_custom_default_channels")
        {
            auto& ctx = Context::instance();
            auto old_default_channels = ctx.default_channels;
            ctx.channel_alias = "https://ali.as/";
            ctx.default_channels = { "prefix" };
            ctx.channels = { "prefix-and-more" };

            ChannelContext channel_context;
            auto base = std::string("https://ali.as/prefix-and-more/");
            auto& chan = channel_context.make_channel(base);
            std::vector<std::string> expected_urls = { base + platform, base + "noarch" };
            CHECK_EQ(chan.urls(), expected_urls);

            ctx.channel_alias = "https://conda.anaconda.org";
            ctx.custom_channels.clear();
            ctx.default_channels = old_default_channels;
        }

        TEST_CASE("custom_channels")
        {
            // ChannelContext builds its custom channels with
            // make_simple_channel
            auto& ctx = Context::instance();
            ctx.channel_alias = "https://mydomain.com/channels/";
            ctx.custom_channels = {
                { "test_channel", "file:///tmp" },
                { "some_channel", "https://conda.mydomain.xyz/" },
            };

            ChannelContext channel_context;
            const auto& ch = channel_context.get_channel_alias();
            CHECK_EQ(ch.scheme(), "https");
            CHECK_EQ(ch.location(), "mydomain.com/channels");
            CHECK_EQ(ch.name(), "<alias>");
            CHECK_EQ(ch.canonical_name(), "<alias>");

            {
                std::string value = "test_channel";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "file");
                CHECK_EQ(c.location(), "/tmp");
                CHECK_EQ(c.name(), "test_channel");
                CHECK_EQ(c.canonical_name(), "test_channel");
                CHECK_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
                std::vector<std::string> exp_urls({ std::string("file:///tmp/test_channel/") + platform,
                                                    std::string("file:///tmp/test_channel/noarch") });
                CHECK_EQ(c.urls(), exp_urls);
            }

            {
                std::string value = "some_channel";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "https");
                CHECK_EQ(c.location(), "conda.mydomain.xyz");
                CHECK_EQ(c.name(), "some_channel");
                CHECK_EQ(c.canonical_name(), "some_channel");
                CHECK_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
                std::vector<std::string> exp_urls(
                    { std::string("https://conda.mydomain.xyz/some_channel/") + platform,
                      std::string("https://conda.mydomain.xyz/some_channel/noarch") }
                );
                CHECK_EQ(c.urls(), exp_urls);
            }

            ctx.channel_alias = "https://conda.anaconda.org";
            ctx.custom_channels.clear();
        }

        TEST_CASE("custom_multichannels")
        {
            // ChannelContext builds its custom channels with
            // make_simple_channel
            auto& ctx = Context::instance();
            ctx.custom_multichannels["xtest"] = std::vector<std::string>{
                "https://mydomain.com/conda-forge",
                "https://mydomain.com/bioconda",
                "https://mydomain.com/snakepit"
            };
            ctx.custom_multichannels["ytest"] = std::vector<std::string>{
                "https://otherdomain.com/conda-forge",
                "https://otherdomain.com/bioconda",
                "https://otherdomain.com/snakepit"
            };

            ChannelContext channel_context;

            auto x = channel_context.get_channels({ "xtest" });

            CHECK_EQ(x.size(), 3);
            auto* c1 = x[0];

            std::vector<std::string> exp_urls(
                { std::string("https://mydomain.com/conda-forge/") + platform,
                  std::string("https://mydomain.com/conda-forge/noarch") }
            );

            CHECK_EQ(c1->urls(), exp_urls);

            std::vector<std::string> exp_urlsy3(
                { std::string("https://otherdomain.com/snakepit/") + platform,
                  std::string("https://otherdomain.com/snakepit/noarch") }
            );

            auto y = channel_context.get_channels({ "ytest" });
            auto* y3 = y[2];

            CHECK_EQ(y3->urls(), exp_urlsy3);

            ctx.channel_alias = "https://conda.anaconda.org";
            ctx.custom_multichannels.clear();
        }

        TEST_CASE("custom_extended_multichannels")
        {
            // ChannelContext builds its custom channels with
            // make_simple_channel
            auto& ctx = Context::instance();

            ctx.channel_alias = "https://condaforge.org/channels/";

            ctx.custom_channels["xyz"] = "https://mydomain.xyz/xyzchannel";

            ctx.custom_multichannels["everything"] = std::vector<std::string>{
                "conda-forge",
                "https://mydomain.com/bioconda",
                "xyz"
            };

            ChannelContext channel_context;

            auto x = channel_context.get_channels({ "everything" });

            CHECK_EQ(x.size(), 3);
            auto* c1 = x[0];
            auto* c2 = x[1];
            auto* c3 = x[2];

            std::vector<std::string> exp_urls(
                { std::string("https://condaforge.org/channels/conda-forge/") + platform,
                  std::string("https://condaforge.org/channels/conda-forge/noarch") }
            );

            CHECK_EQ(c1->urls(), exp_urls);

            std::vector<std::string> exp_urls2(
                { std::string("https://mydomain.com/bioconda/") + platform,
                  std::string("https://mydomain.com/bioconda/noarch") }
            );

            CHECK_EQ(c2->urls(), exp_urls2);

            std::vector<std::string> exp_urls3(
                { std::string("https://mydomain.xyz/xyzchannel/xyz/") + platform,
                  std::string("https://mydomain.xyz/xyzchannel/xyz/noarch") }
            );

            CHECK_EQ(c3->urls(), exp_urls3);

            ctx.channel_alias = "https://conda.anaconda.org";
            ctx.custom_multichannels.clear();
            ctx.custom_channels.clear();
        }

        TEST_CASE("default_channels")
        {
            auto& ctx = Context::instance();
            ChannelContext channel_context;

            auto x = channel_context.get_channels({ "defaults" });
#if !defined(_WIN32)
            const Channel* c1 = x[0];
            const Channel* c2 = x[1];

            CHECK_EQ(c1->name(), "pkgs/main");
            std::vector<std::string> exp_urls(
                { std::string("https://repo.anaconda.com/pkgs/main/") + platform,
                  std::string("https://repo.anaconda.com/pkgs/main/noarch") }
            );
            CHECK_EQ(c1->urls(), exp_urls);

            CHECK_EQ(c2->name(), "pkgs/r");
            std::vector<std::string> exp_urls2(
                { std::string("https://repo.anaconda.com/pkgs/r/") + platform,
                  std::string("https://repo.anaconda.com/pkgs/r/noarch") }
            );
            CHECK_EQ(c2->urls(), exp_urls2);

            CHECK_EQ(c1->location(), "repo.anaconda.com");
            CHECK_EQ(c1->scheme(), "https");

#endif
            ctx.custom_channels.clear();
        }

        TEST_CASE("custom_default_channels")
        {
            auto& ctx = Context::instance();
            ctx.default_channels = { "https://mamba.com/test/channel",
                                     "https://mamba.com/stable/channel" };
            ChannelContext channel_context;

            auto x = channel_context.get_channels({ "defaults" });
            const Channel* c1 = x[0];
            const Channel* c2 = x[1];

            CHECK_EQ(c1->name(), "test/channel");
            std::vector<std::string> exp_urls(
                { std::string("https://mamba.com/test/channel/") + platform,
                  std::string("https://mamba.com/test/channel/noarch") }
            );
            CHECK_EQ(c1->urls(), exp_urls);
            std::vector<std::string> exp_urls2(
                { std::string("https://mamba.com/stable/channel/") + platform,
                  std::string("https://mamba.com/stable/channel/noarch") }
            );
            CHECK_EQ(c2->urls(), exp_urls2);

            CHECK_EQ(c2->name(), "stable/channel");
            CHECK_EQ(c2->location(), "mamba.com");
            CHECK_EQ(c2->scheme(), "https");

            ctx.custom_channels.clear();
        }

        TEST_CASE("custom_channels_with_labels")
        {
            auto& ctx = Context::instance();
            ctx.custom_channels = {
                { "test_channel", "https://server.com/private/channels" },
                { "random/test_channel", "https://server.com/random/channels" },
            };
            ChannelContext channel_context;

            {
                std::string value = "test_channel";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "https");
                CHECK_EQ(c.location(), "server.com/private/channels");
                CHECK_EQ(c.name(), "test_channel");
                CHECK_EQ(c.canonical_name(), "test_channel");
                CHECK_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
                std::vector<std::string> exp_urls(
                    { std::string("https://server.com/private/channels/test_channel/") + platform,
                      std::string("https://server.com/private/channels/test_channel/noarch") }
                );
                CHECK_EQ(c.urls(), exp_urls);
            }

            {
                std::string value = "test_channel/mylabel/xyz";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "https");
                CHECK_EQ(c.location(), "server.com/private/channels");
                CHECK_EQ(c.name(), "test_channel/mylabel/xyz");
                CHECK_EQ(c.canonical_name(), "test_channel/mylabel/xyz");
                CHECK_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
                std::vector<std::string> exp_urls(
                    { std::string("https://server.com/private/channels/test_channel/mylabel/xyz/")
                          + platform,
                      std::string("https://server.com/private/channels/test_channel/mylabel/xyz/noarch"
                      ) }
                );
                CHECK_EQ(c.urls(), exp_urls);
            }

            {
                std::string value = "random/test_channel/pkg";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "https");
                CHECK_EQ(c.location(), "server.com/random/channels");
                CHECK_EQ(c.name(), "random/test_channel/pkg");
                CHECK_EQ(c.canonical_name(), "random/test_channel/pkg");
                CHECK_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
                std::vector<std::string> exp_urls(
                    { std::string("https://server.com/random/channels/random/test_channel/pkg/")
                          + platform,
                      std::string("https://server.com/random/channels/random/test_channel/pkg/noarch") }
                );
                CHECK_EQ(c.urls(), exp_urls);
            }

            ctx.channel_alias = "https://conda.anaconda.org";
            ctx.custom_channels.clear();
        }
    }

    TEST_SUITE("Channel")
    {
        TEST_CASE("channel_name")
        {
            std::string value = "https://repo.mamba.pm/conda-forge";
            ChannelContext channel_context;
            const Channel& c = channel_context.make_channel(value);
            CHECK_EQ(c.scheme(), "https");
            CHECK_EQ(c.location(), "repo.mamba.pm");
            CHECK_EQ(c.name(), "conda-forge");
            CHECK_EQ(c.canonical_name(), "https://repo.mamba.pm/conda-forge");
            CHECK_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));
        }

        TEST_CASE("make_channel")
        {
            std::string value = "conda-forge";
            ChannelContext channel_context;
            const Channel& c = channel_context.make_channel(value);
            CHECK_EQ(c.scheme(), "https");
            CHECK_EQ(c.location(), "conda.anaconda.org");
            CHECK_EQ(c.name(), "conda-forge");
            CHECK_EQ(c.canonical_name(), "conda-forge");
            CHECK_EQ(c.platforms(), std::vector<std::string>({ platform, "noarch" }));

            std::string value2 = "https://repo.anaconda.com/pkgs/main[" + platform + "]";
            const Channel& c2 = channel_context.make_channel(value2);
            CHECK_EQ(c2.scheme(), "https");
            CHECK_EQ(c2.location(), "repo.anaconda.com");
            CHECK_EQ(c2.name(), "pkgs/main");
            CHECK_EQ(c2.canonical_name(), "https://repo.anaconda.com/pkgs/main");
            CHECK_EQ(c2.platforms(), std::vector<std::string>({ platform }));

            std::string value3 = "https://conda.anaconda.org/conda-forge[" + platform + "]";
            const Channel& c3 = channel_context.make_channel(value3);
            CHECK_EQ(c3.scheme(), c.scheme());
            CHECK_EQ(c3.location(), c.location());
            CHECK_EQ(c3.name(), c.name());
            CHECK_EQ(c3.canonical_name(), c.canonical_name());
            CHECK_EQ(c3.platforms(), std::vector<std::string>({ platform }));

            std::string value4 = "/home/mamba/test/channel_b";
            const Channel& c4 = channel_context.make_channel(value4);
            CHECK_EQ(c4.scheme(), "file");
#ifdef _WIN32
            std::string driveletter = fs::absolute(fs::u8path("/")).string().substr(0, 1);
            CHECK_EQ(c4.location(), driveletter + ":/home/mamba/test");
            CHECK_EQ(c4.canonical_name(), "file:///" + driveletter + ":/home/mamba/test/channel_b");
#else
            CHECK_EQ(c4.location(), "/home/mamba/test");
            CHECK_EQ(c4.canonical_name(), "file:///home/mamba/test/channel_b");
#endif
            CHECK_EQ(c4.name(), "channel_b");
            CHECK_EQ(c4.platforms(), std::vector<std::string>({ platform, "noarch" }));

            std::string value5 = "/home/mamba/test/channel_b[" + platform + "]";
            const Channel& c5 = channel_context.make_channel(value5);
            CHECK_EQ(c5.scheme(), "file");
#ifdef _WIN32
            CHECK_EQ(c5.location(), driveletter + ":/home/mamba/test");
            CHECK_EQ(c5.canonical_name(), "file:///" + driveletter + ":/home/mamba/test/channel_b");
#else
            CHECK_EQ(c5.location(), "/home/mamba/test");
            CHECK_EQ(c5.canonical_name(), "file:///home/mamba/test/channel_b");
#endif
            CHECK_EQ(c5.name(), "channel_b");
            CHECK_EQ(c5.platforms(), std::vector<std::string>({ platform }));

            std::string value6a = "http://localhost:8000/conda-forge[noarch]";
            const Channel& c6a = channel_context.make_channel(value6a);
            CHECK_EQ(
                c6a.urls(false),
                std::vector<std::string>({ "http://localhost:8000/conda-forge/noarch" })
            );

            std::string value6b = "http://localhost:8000/conda_mirror/conda-forge[noarch]";
            const Channel& c6b = channel_context.make_channel(value6b);
            CHECK_EQ(
                c6b.urls(false),
                std::vector<std::string>({ "http://localhost:8000/conda_mirror/conda-forge/noarch" })
            );

            std::string value7 = "conda-forge[noarch,arbitrary]";
            const Channel& c7 = channel_context.make_channel(value7);
            CHECK_EQ(c7.platforms(), std::vector<std::string>({ "noarch", "arbitrary" }));
        }

        TEST_CASE("urls")
        {
            std::string value = "https://conda.anaconda.org/conda-forge[noarch,win-64,arbitrary]";
            ChannelContext channel_context;
            const Channel& c = channel_context.make_channel(value);
            CHECK_EQ(
                c.urls(),
                std::vector<std::string>({ "https://conda.anaconda.org/conda-forge/noarch",
                                           "https://conda.anaconda.org/conda-forge/win-64",
                                           "https://conda.anaconda.org/conda-forge/arbitrary" })
            );

            const Channel& c1 = channel_context.make_channel("https://conda.anaconda.org/conda-forge");
            CHECK_EQ(
                c1.urls(),
                std::vector<std::string>({ "https://conda.anaconda.org/conda-forge/" + platform,
                                           "https://conda.anaconda.org/conda-forge/noarch" })
            );
        }

        TEST_CASE("add_token")
        {
            auto& ctx = Context::instance();
            ctx.authentication_info()["conda.anaconda.org"] = AuthenticationInfo{
                AuthenticationType::kCondaToken,
                "my-12345-token"
            };

            ChannelContext channel_context;

            const auto& chan = channel_context.make_channel("conda-forge[noarch]");
            CHECK_EQ(chan.token(), "my-12345-token");
            CHECK_EQ(
                chan.urls(true),
                std::vector<std::string>{
                    { "https://conda.anaconda.org/t/my-12345-token/conda-forge/noarch" } }
            );
            CHECK_EQ(
                chan.urls(false),
                std::vector<std::string>{ { "https://conda.anaconda.org/conda-forge/noarch" } }
            );
        }

        TEST_CASE("add_multiple_tokens")
        {
            auto& ctx = Context::instance();
            ctx.authentication_info()["conda.anaconda.org"] = AuthenticationInfo{
                AuthenticationType::kCondaToken,
                "base-token"
            };
            ctx.authentication_info()["conda.anaconda.org/conda-forge"] = AuthenticationInfo{
                AuthenticationType::kCondaToken,
                "channel-token"
            };

            ChannelContext channel_context;

            const auto& chan = channel_context.make_channel("conda-forge[noarch]");
            CHECK_EQ(chan.token(), "channel-token");
        }

        TEST_CASE("fix_win_file_path")
        {
            ChannelContext channel_context;
            if (platform == "win-64")
            {
                const Channel& c = channel_context.make_channel("C:\\test\\channel");
                CHECK_EQ(
                    c.urls(false),
                    std::vector<std::string>({ "file:///C:/test/channel/win-64",
                                               "file:///C:/test/channel/noarch" })
                );
            }
            else
            {
                const Channel& c = channel_context.make_channel("/test/channel");
                CHECK_EQ(
                    c.urls(false),
                    std::vector<std::string>({ std::string("file:///test/channel/") + platform,
                                               "file:///test/channel/noarch" })
                );
            }
        }

        TEST_CASE("trailing_slash")
        {
            ChannelContext channel_context;
            const Channel& c = channel_context.make_channel("http://localhost:8000/");
            CHECK_EQ(c.platform_url("win-64", false), "http://localhost:8000/win-64");
            CHECK_EQ(c.base_url(), "http://localhost:8000");
            std::vector<std::string> expected_urls({ std::string("http://localhost:8000/") + platform,
                                                     "http://localhost:8000/noarch" });
            CHECK_EQ(c.urls(true), expected_urls);
            const Channel& c4 = channel_context.make_channel("http://localhost:8000");
            CHECK_EQ(c4.platform_url("linux-64", false), "http://localhost:8000/linux-64");
            const Channel& c2 = channel_context.make_channel("http://user:test@localhost:8000/");
            CHECK_EQ(c2.platform_url("win-64", false), "http://localhost:8000/win-64");
            CHECK_EQ(c2.platform_url("win-64", true), "http://user:test@localhost:8000/win-64");
            const Channel& c3 = channel_context.make_channel(
                "https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012"
            );
            CHECK_EQ(c3.platform_url("win-64", false), "https://localhost:8000/win-64");
            CHECK_EQ(
                c3.platform_url("win-64", true),
                "https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012/win-64"
            );

            std::vector<std::string> expected_urls2(
                { std::string("https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012/")
                      + platform,
                  "https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012/noarch" }
            );

            CHECK_EQ(c3.urls(true), expected_urls2);
        }

        TEST_CASE("load_tokens")
        {
            // touch(env::home_directory() / ".continuum" / "anaconda")
            // auto& ctx = Context::instance();
            // ctx.channel_tokens["https://conda.anaconda.org"] = "my-12345-token";

            // ChannelContext channel_context;

            // const auto& chan = channel_context.make_channel("conda-forge");
            // CHECK_EQ(chan.token(), "my-12345-token");
            // CHECK_EQ(chan.url(true),
            // "https://conda.anaconda.org/t/my-12345-token/conda-forge/noarch");
            // CHECK_EQ(chan.url(false), "https://conda.anaconda.org/conda-forge/noarch");
        }

        TEST_CASE("split_platform")
        {
            std::string platform_found, cleaned_url;
            split_platform(
                { "noarch", "linux-64" },
                "https://mamba.com/linux-64/package.tar.bz2",
                cleaned_url,
                platform_found
            );

            CHECK_EQ(platform_found, "linux-64");
            CHECK_EQ(cleaned_url, "https://mamba.com/package.tar.bz2");

            split_platform(
                { "noarch", "linux-64" },
                "https://mamba.com/linux-64/noarch-package.tar.bz2",
                cleaned_url,
                platform_found
            );
            CHECK_EQ(platform_found, "linux-64");
            CHECK_EQ(cleaned_url, "https://mamba.com/noarch-package.tar.bz2");

            split_platform(
                { "linux-64", "osx-arm64", "noarch" },
                "https://mamba.com/noarch/kernel_linux-64-package.tar.bz2",
                cleaned_url,
                platform_found
            );
            CHECK_EQ(platform_found, "noarch");
            CHECK_EQ(cleaned_url, "https://mamba.com/kernel_linux-64-package.tar.bz2");

            split_platform(
                { "noarch", "linux-64" },
                "https://mamba.com/linux-64",
                cleaned_url,
                platform_found
            );

            CHECK_EQ(platform_found, "linux-64");
            CHECK_EQ(cleaned_url, "https://mamba.com");

            split_platform(
                { "noarch", "linux-64" },
                "https://mamba.com/noarch",
                cleaned_url,
                platform_found
            );

            CHECK_EQ(platform_found, "noarch");
            CHECK_EQ(cleaned_url, "https://mamba.com");
        }
    }
}  // namespace mamba
