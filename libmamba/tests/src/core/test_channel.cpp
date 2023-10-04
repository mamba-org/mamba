// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <type_traits>

#include <doctest/doctest.h>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/util/flat_set.hpp"

#include "mambatests.hpp"

namespace mamba
{

    static const std::string platform = std::string(specs::build_platform_name());
    using PlatformSet = typename util::flat_set<std::string>;
    using UrlSet = typename util::flat_set<std::string>;

    static_assert(std::is_move_constructible_v<mamba::Channel>);
    static_assert(std::is_move_assignable_v<mamba::Channel>);

    TEST_SUITE("ChannelContext")
    {
        TEST_CASE("init")
        {
            // ChannelContext builds its custom channels with
            // make_simple_channel
            ChannelContext channel_context{ mambatests::context() };
            const auto& ch = channel_context.get_channel_alias();
            CHECK_EQ(ch.str(), "https://conda.anaconda.org/");

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
            auto& ctx = mambatests::context();
            ctx.channel_alias = "https://mydomain.com/channels/";

            ChannelContext channel_context{ mambatests::context() };

            const auto& ch = channel_context.get_channel_alias();
            CHECK_EQ(ch.str(), "https://mydomain.com/channels/");

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
            CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));

            ctx.channel_alias = "https://conda.anaconda.org";
        }

        // Regression test for https://github.com/mamba-org/mamba/issues/1671
        TEST_CASE("channel_alias_with_custom_default_channels")
        {
            auto& ctx = mambatests::context();
            auto old_default_channels = ctx.default_channels;
            ctx.channel_alias = "https://ali.as/";
            ctx.default_channels = { "prefix" };
            ctx.channels = { "prefix-and-more" };

            ChannelContext channel_context{ ctx };
            auto base = std::string("https://ali.as/prefix-and-more/");
            auto& chan = channel_context.make_channel(base);
            CHECK_EQ(chan.urls(), UrlSet{ base + platform, base + "noarch" });

            ctx.channel_alias = "https://conda.anaconda.org";
            ctx.custom_channels.clear();
            ctx.default_channels = old_default_channels;
        }

        TEST_CASE("custom_channels")
        {
            // ChannelContext builds its custom channels with
            // make_simple_channel
            auto& ctx = mambatests::context();
            ctx.channel_alias = "https://mydomain.com/channels/";
            ctx.custom_channels = {
                { "test_channel", "file:///tmp" },
                { "some_channel", "https://conda.mydomain.xyz/" },
            };

            ChannelContext channel_context{ ctx };
            const auto& ch = channel_context.get_channel_alias();
            CHECK_EQ(ch.str(), "https://mydomain.com/channels/");

            {
                std::string value = "test_channel";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "file");
                CHECK_EQ(c.location(), "/tmp");
                CHECK_EQ(c.name(), "test_channel");
                CHECK_EQ(c.canonical_name(), "test_channel");
                CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));
                const UrlSet exp_urls({
                    std::string("file:///tmp/test_channel/") + platform,
                    "file:///tmp/test_channel/noarch",
                });
                CHECK_EQ(c.urls(), exp_urls);
            }

            {
                std::string value = "some_channel";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "https");
                CHECK_EQ(c.location(), "conda.mydomain.xyz");
                CHECK_EQ(c.name(), "some_channel");
                CHECK_EQ(c.canonical_name(), "some_channel");
                CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));
                const UrlSet exp_urls({
                    std::string("https://conda.mydomain.xyz/some_channel/") + platform,
                    "https://conda.mydomain.xyz/some_channel/noarch",
                });
                CHECK_EQ(c.urls(), exp_urls);
            }

            ctx.channel_alias = "https://conda.anaconda.org";
            ctx.custom_channels.clear();
        }

        TEST_CASE("custom_multichannels")
        {
            // ChannelContext builds its custom channels with
            // make_simple_channel
            auto& ctx = mambatests::context();
            ctx.custom_multichannels["xtest"] = std::vector<std::string>{
                "https://mydomain.com/conda-forge",
                "https://mydomain.com/bioconda",
                "https://mydomain.com/snakepit",
            };
            ctx.custom_multichannels["ytest"] = std::vector<std::string>{
                "https://otherdomain.com/conda-forge",
                "https://otherdomain.com/bioconda",
                "https://otherdomain.com/snakepit",
            };

            ChannelContext channel_context{ ctx };

            auto x = channel_context.get_channels({ "xtest" });

            CHECK_EQ(x.size(), 3);
            auto* c1 = x[0];

            const UrlSet exp_urls({
                std::string("https://mydomain.com/conda-forge/") + platform,
                "https://mydomain.com/conda-forge/noarch",
            });

            CHECK_EQ(c1->urls(), exp_urls);

            const UrlSet exp_urlsy3({
                std::string("https://otherdomain.com/snakepit/") + platform,
                "https://otherdomain.com/snakepit/noarch",
            });

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
            auto& ctx = mambatests::context();

            ctx.channel_alias = "https://condaforge.org/channels/";

            ctx.custom_channels["xyz"] = "https://mydomain.xyz/xyzchannel";

            ctx.custom_multichannels["everything"] = std::vector<std::string>{
                "conda-forge",
                "https://mydomain.com/bioconda",
                "xyz"
            };

            ChannelContext channel_context{ ctx };

            auto x = channel_context.get_channels({ "everything" });

            CHECK_EQ(x.size(), 3);
            auto* c1 = x[0];
            auto* c2 = x[1];
            auto* c3 = x[2];

            const UrlSet exp_urls({
                std::string("https://condaforge.org/channels/conda-forge/") + platform,
                "https://condaforge.org/channels/conda-forge/noarch",
            });

            CHECK_EQ(c1->urls(), exp_urls);

            const UrlSet exp_urls2({
                std::string("https://mydomain.com/bioconda/") + platform,
                "https://mydomain.com/bioconda/noarch",
            });

            CHECK_EQ(c2->urls(), exp_urls2);

            const UrlSet exp_urls3({
                std::string("https://mydomain.xyz/xyzchannel/xyz/") + platform,
                "https://mydomain.xyz/xyzchannel/xyz/noarch",
            });

            CHECK_EQ(c3->urls(), exp_urls3);

            ctx.channel_alias = "https://conda.anaconda.org";
            ctx.custom_multichannels.clear();
            ctx.custom_channels.clear();
        }

        TEST_CASE("default_channels")
        {
            auto& ctx = mambatests::context();
            ChannelContext channel_context{ ctx };

            auto x = channel_context.get_channels({ "defaults" });
#if !defined(_WIN32)
            const Channel* c1 = x[0];
            const Channel* c2 = x[1];

            CHECK_EQ(c1->name(), "pkgs/main");
            const UrlSet exp_urls({
                std::string("https://repo.anaconda.com/pkgs/main/") + platform,
                "https://repo.anaconda.com/pkgs/main/noarch",
            });
            CHECK_EQ(c1->urls(), exp_urls);

            CHECK_EQ(c2->name(), "pkgs/r");
            const UrlSet exp_urls2({
                std::string("https://repo.anaconda.com/pkgs/r/") + platform,
                "https://repo.anaconda.com/pkgs/r/noarch",
            });
            CHECK_EQ(c2->urls(), exp_urls2);

            CHECK_EQ(c1->location(), "repo.anaconda.com");
            CHECK_EQ(c1->scheme(), "https");

#endif
            ctx.custom_channels.clear();
        }

        TEST_CASE("custom_default_channels")
        {
            auto& ctx = mambatests::context();
            ctx.default_channels = {
                "https://mamba.com/test/channel",
                "https://mamba.com/stable/channel",
            };
            ChannelContext channel_context{ ctx };

            auto x = channel_context.get_channels({ "defaults" });
            const Channel* c1 = x[0];
            const Channel* c2 = x[1];

            CHECK_EQ(c1->name(), "test/channel");
            const UrlSet exp_urls({
                std::string("https://mamba.com/test/channel/") + platform,
                "https://mamba.com/test/channel/noarch",
            });
            CHECK_EQ(c1->urls(), exp_urls);
            const UrlSet exp_urls2({
                std::string("https://mamba.com/stable/channel/") + platform,
                "https://mamba.com/stable/channel/noarch",
            });
            CHECK_EQ(c2->urls(), exp_urls2);

            CHECK_EQ(c2->name(), "stable/channel");
            CHECK_EQ(c2->location(), "mamba.com");
            CHECK_EQ(c2->scheme(), "https");

            ctx.custom_channels.clear();
        }

        TEST_CASE("custom_local_channels")
        {
            auto& ctx = mambatests::context();

            // Create conda-bld directory to enable testing
            auto conda_bld_dir = env::home_directory() / "conda-bld";
            bool to_be_removed = fs::create_directories(conda_bld_dir);

            ChannelContext channel_context{ ctx };

            const auto& custom = channel_context.get_custom_channels();

            CHECK_EQ(custom.size(), 4);

            auto it = custom.find("conda-bld");
            CHECK_NE(it, custom.end());
            CHECK_EQ(it->second.name(), "conda-bld");
            CHECK_EQ(it->second.location(), env::home_directory());
            CHECK_EQ(it->second.canonical_name(), "local");
            CHECK_EQ(it->second.scheme(), "file");

            auto local_channels = channel_context.get_channels({ "local" });
            CHECK_EQ(local_channels.size(), 1);

            // Cleaning
            ctx.custom_channels.clear();
            if (to_be_removed)
            {
                fs::remove_all(conda_bld_dir);
            }
        }

        TEST_CASE("custom_channels_with_labels")
        {
            auto& ctx = mambatests::context();
            ctx.custom_channels = {
                { "test_channel", "https://server.com/private/channels" },
                { "random/test_channel", "https://server.com/random/channels" },
            };
            ChannelContext channel_context{ ctx };

            {
                std::string value = "test_channel";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "https");
                CHECK_EQ(c.location(), "server.com/private/channels");
                CHECK_EQ(c.name(), "test_channel");
                CHECK_EQ(c.canonical_name(), "test_channel");
                CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));
                const UrlSet exp_urls({
                    std::string("https://server.com/private/channels/test_channel/") + platform,
                    "https://server.com/private/channels/test_channel/noarch",
                });
                CHECK_EQ(c.urls(), exp_urls);
            }

            {
                std::string value = "test_channel/mylabel/xyz";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "https");
                CHECK_EQ(c.location(), "server.com/private/channels");
                CHECK_EQ(c.name(), "test_channel/mylabel/xyz");
                CHECK_EQ(c.canonical_name(), "test_channel/mylabel/xyz");
                CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));
                const UrlSet exp_urls({
                    std::string("https://server.com/private/channels/test_channel/mylabel/xyz/")
                        + platform,
                    "https://server.com/private/channels/test_channel/mylabel/xyz/noarch",
                });
                CHECK_EQ(c.urls(), exp_urls);
            }

            {
                std::string value = "random/test_channel/pkg";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(c.scheme(), "https");
                CHECK_EQ(c.location(), "server.com/random/channels");
                CHECK_EQ(c.name(), "random/test_channel/pkg");
                CHECK_EQ(c.canonical_name(), "random/test_channel/pkg");
                CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));
                const UrlSet exp_urls({
                    std::string("https://server.com/random/channels/random/test_channel/pkg/")
                        + platform,
                    "https://server.com/random/channels/random/test_channel/pkg/noarch",
                });
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
            ChannelContext channel_context{ mambatests::context() };
            const Channel& c = channel_context.make_channel(value);
            CHECK_EQ(c.scheme(), "https");
            CHECK_EQ(c.location(), "repo.mamba.pm");
            CHECK_EQ(c.name(), "conda-forge");
            CHECK_EQ(c.canonical_name(), "https://repo.mamba.pm/conda-forge");
            CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));
        }

        TEST_CASE("make_channel")
        {
            std::string value = "conda-forge";
            ChannelContext channel_context{ mambatests::context() };
            const Channel& c = channel_context.make_channel(value);
            CHECK_EQ(c.scheme(), "https");
            CHECK_EQ(c.location(), "conda.anaconda.org");
            CHECK_EQ(c.name(), "conda-forge");
            CHECK_EQ(c.canonical_name(), "conda-forge");
            CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));

            std::string value2 = "https://repo.anaconda.com/pkgs/main[" + platform + "]";
            const Channel& c2 = channel_context.make_channel(value2);
            CHECK_EQ(c2.scheme(), "https");
            CHECK_EQ(c2.location(), "repo.anaconda.com");
            CHECK_EQ(c2.name(), "pkgs/main");
            CHECK_EQ(c2.canonical_name(), "https://repo.anaconda.com/pkgs/main");
            CHECK_EQ(c2.platforms(), PlatformSet({ platform }));

            std::string value3 = "https://conda.anaconda.org/conda-forge[" + platform + "]";
            const Channel& c3 = channel_context.make_channel(value3);
            CHECK_EQ(c3.scheme(), c.scheme());
            CHECK_EQ(c3.location(), c.location());
            CHECK_EQ(c3.name(), c.name());
            CHECK_EQ(c3.canonical_name(), c.canonical_name());
            CHECK_EQ(c3.platforms(), PlatformSet({ platform }));

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
            CHECK_EQ(c4.platforms(), PlatformSet({ platform, "noarch" }));

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
            CHECK_EQ(c5.platforms(), PlatformSet({ platform }));

            std::string value6a = "http://localhost:8000/conda-forge[noarch]";
            const Channel& c6a = channel_context.make_channel(value6a);
            CHECK_EQ(c6a.urls(false), UrlSet({ "http://localhost:8000/conda-forge/noarch" }));

            std::string value6b = "http://localhost:8000/conda_mirror/conda-forge[noarch]";
            const Channel& c6b = channel_context.make_channel(value6b);
            CHECK_EQ(
                c6b.urls(false),
                UrlSet({ "http://localhost:8000/conda_mirror/conda-forge/noarch" })
            );

            std::string value7 = "conda-forge[noarch,arbitrary]";
            const Channel& c7 = channel_context.make_channel(value7);
            CHECK_EQ(c7.platforms(), PlatformSet({ "noarch", "arbitrary" }));
        }

        TEST_CASE("urls")
        {
            std::string value = "https://conda.anaconda.org/conda-forge[noarch,win-64,arbitrary]";
            ChannelContext channel_context{ mambatests::context() };
            const Channel& c = channel_context.make_channel(value);
            CHECK_EQ(
                c.urls(),
                UrlSet({
                    "https://conda.anaconda.org/conda-forge/arbitrary",
                    "https://conda.anaconda.org/conda-forge/noarch",
                    "https://conda.anaconda.org/conda-forge/win-64",
                })
            );

            const Channel& c1 = channel_context.make_channel("https://conda.anaconda.org/conda-forge");
            CHECK_EQ(
                c1.urls(),
                UrlSet({
                    "https://conda.anaconda.org/conda-forge/" + platform,
                    "https://conda.anaconda.org/conda-forge/noarch",
                })
            );
        }

        TEST_CASE("add_token")
        {
            auto& ctx = mambatests::context();
            ctx.authentication_info()["conda.anaconda.org"] = specs::CondaToken{ "my-12345-token" };

            ChannelContext channel_context{ ctx };

            const auto& chan = channel_context.make_channel("conda-forge[noarch]");
            CHECK_EQ(chan.token(), "my-12345-token");
            CHECK_EQ(
                chan.urls(true),
                UrlSet({ "https://conda.anaconda.org/t/my-12345-token/conda-forge/noarch" })
            );
            CHECK_EQ(chan.urls(false), UrlSet({ "https://conda.anaconda.org/conda-forge/noarch" }));
        }

        TEST_CASE("add_multiple_tokens")
        {
            auto& ctx = mambatests::context();
            ctx.authentication_info()["conda.anaconda.org"] = specs::CondaToken{ "base-token" };
            ctx.authentication_info()["conda.anaconda.org/conda-forge"] = specs::CondaToken{
                "channel-token"
            };

            ChannelContext channel_context{ ctx };

            const auto& chan = channel_context.make_channel("conda-forge[noarch]");
            CHECK_EQ(chan.token(), "channel-token");
        }

        TEST_CASE("fix_win_file_path")
        {
            ChannelContext channel_context{ mambatests::context() };
            if (platform == "win-64")
            {
                const Channel& c = channel_context.make_channel("C:\\test\\channel");
                CHECK_EQ(
                    c.urls(false),
                    UrlSet({ "file:///C:/test/channel/win-64", "file:///C:/test/channel/noarch" })
                );
            }
            else
            {
                const Channel& c = channel_context.make_channel("/test/channel");
                CHECK_EQ(
                    c.urls(false),
                    UrlSet({ std::string("file:///test/channel/") + platform,
                             "file:///test/channel/noarch" })
                );
            }
        }

        TEST_CASE("trailing_slash")
        {
            ChannelContext channel_context{ mambatests::context() };
            const Channel& c = channel_context.make_channel("http://localhost:8000/");
            CHECK_EQ(c.platform_url("win-64", false), "http://localhost:8000/win-64");
            CHECK_EQ(c.base_url(), "http://localhost:8000");
            const UrlSet expected_urls({ std::string("http://localhost:8000/") + platform,
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

            const UrlSet expected_urls2({
                std::string("https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012/")
                    + platform,
                "https://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012/noarch",
            });

            CHECK_EQ(c3.urls(true), expected_urls2);
        }

        TEST_CASE("load_tokens")
        {
            // touch(env::home_directory() / ".continuum" / "anaconda")
            // auto& ctx = mambatests::context();
            // ctx.channel_tokens["https://conda.anaconda.org"] = "my-12345-token";

            // ChannelContext channel_context;

            // const auto& chan = channel_context.make_channel("conda-forge");
            // CHECK_EQ(chan.token(), "my-12345-token");
            // CHECK_EQ(chan.url(true),
            // "https://conda.anaconda.org/t/my-12345-token/conda-forge/noarch");
            // CHECK_EQ(chan.url(false), "https://conda.anaconda.org/conda-forge/noarch");
        }
    }
}  // namespace mamba
