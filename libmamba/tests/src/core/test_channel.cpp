// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <type_traits>

#include <doctest/doctest.h>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/flat_set.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

#include "doctest-printer/conda_url.hpp"
#include "doctest-printer/flat_set.hpp"

#include "mambatests.hpp"

namespace mamba
{

    static const std::string platform = std::string(specs::build_platform_name());
    using PlatformSet = typename util::flat_set<std::string>;
    using UrlSet = typename util::flat_set<std::string>;
    using CondaURL = specs::CondaURL;

    static_assert(std::is_move_constructible_v<mamba::Channel>);
    static_assert(std::is_move_assignable_v<mamba::Channel>);


    TEST_SUITE("ChannelContext")
    {
        TEST_CASE("init")
        {
            // ChannelContext builds its custom channels with
            // make_simple_channel
            auto& ctx = mambatests::context();

            // Hard coded Anaconda channels names set in configuration after refactor
            // Should be moved to test_config
            // FIXME: this has side effect on all tests
            ctx.custom_channels.emplace("pkgs/main", "https://repo.anaconda.com/pkgs/main");
            ctx.custom_channels.emplace("pkgs/r", "https://repo.anaconda.com/pkgs/r");
            ctx.custom_channels.emplace("pkgs/pro", "https://repo.anaconda.com/pkgs/pro");

            ChannelContext channel_context{ ctx };
            const auto& ch = channel_context.get_channel_alias();
            CHECK_EQ(ch.str(), "https://conda.anaconda.org/");

            const auto& custom = channel_context.get_custom_channels();

            auto it = custom.find("pkgs/main");
            REQUIRE_NE(it, custom.end());
            CHECK_EQ(it->second.url(), CondaURL::parse("https://repo.anaconda.com/pkgs/main"));
            CHECK_EQ(it->second.display_name(), "pkgs/main");

            it = custom.find("pkgs/pro");
            REQUIRE_NE(it, custom.end());
            CHECK_EQ(it->second.url(), CondaURL::parse("https://repo.anaconda.com/pkgs/pro"));
            CHECK_EQ(it->second.display_name(), "pkgs/pro");

            it = custom.find("pkgs/r");
            REQUIRE_NE(it, custom.end());
            CHECK_EQ(it->second.url(), CondaURL::parse("https://repo.anaconda.com/pkgs/r"));
            CHECK_EQ(it->second.display_name(), "pkgs/r");
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
            REQUIRE_NE(it, custom.end());
            CHECK_EQ(it->second.url(), CondaURL::parse("https://repo.anaconda.com/pkgs/main"));
            CHECK_EQ(it->second.display_name(), "pkgs/main");

            std::string value = "conda-forge";
            const Channel& c = channel_context.make_channel(value);
            CHECK_EQ(c.url(), CondaURL::parse("https://mydomain.com/channels/conda-forge"));
            CHECK_EQ(c.display_name(), "conda-forge");
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
                CHECK_EQ(c.url(), CondaURL::parse("file:///tmp/test_channel"));
                CHECK_EQ(c.display_name(), "test_channel");
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
                CHECK_EQ(c.url(), CondaURL::parse("https://conda.mydomain.xyz/some_channel"));
                CHECK_EQ(c.display_name(), "some_channel");
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
            auto c1 = x[0];

            const UrlSet exp_urls({
                std::string("https://mydomain.com/conda-forge/") + platform,
                "https://mydomain.com/conda-forge/noarch",
            });

            CHECK_EQ(c1.urls(), exp_urls);

            const UrlSet exp_urlsy3({
                std::string("https://otherdomain.com/snakepit/") + platform,
                "https://otherdomain.com/snakepit/noarch",
            });

            auto y = channel_context.get_channels({ "ytest" });
            auto y3 = y[2];

            CHECK_EQ(y3.urls(), exp_urlsy3);

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
            auto c1 = x[0];
            auto c2 = x[1];
            auto c3 = x[2];

            const UrlSet exp_urls({
                std::string("https://condaforge.org/channels/conda-forge/") + platform,
                "https://condaforge.org/channels/conda-forge/noarch",
            });

            CHECK_EQ(c1.urls(), exp_urls);

            const UrlSet exp_urls2({
                std::string("https://mydomain.com/bioconda/") + platform,
                "https://mydomain.com/bioconda/noarch",
            });

            CHECK_EQ(c2.urls(), exp_urls2);

            const UrlSet exp_urls3({
                std::string("https://mydomain.xyz/xyzchannel/xyz/") + platform,
                "https://mydomain.xyz/xyzchannel/xyz/noarch",
            });

            CHECK_EQ(c3.urls(), exp_urls3);

            ctx.channel_alias = "https://conda.anaconda.org";
            ctx.custom_multichannels.clear();
            ctx.custom_channels.clear();
        }

        TEST_CASE("default_channels")
        {
            auto& ctx = mambatests::context();
            // Hard coded Anaconda multi channel names set in configuration after refactor
            // Should be moved to test_config
            // FIXME: this has side effect on all tests
            ctx.custom_multichannels["defaults"] = ctx.default_channels;

            ChannelContext channel_context{ ctx };

            auto x = channel_context.get_channels({ "defaults" });
#if !defined(_WIN32)
            const Channel c1 = x[0];
            const Channel c2 = x[1];

            CHECK_EQ(c1.url(), CondaURL::parse("https://repo.anaconda.com/pkgs/main"));
            const UrlSet exp_urls({
                std::string("https://repo.anaconda.com/pkgs/main/") + platform,
                "https://repo.anaconda.com/pkgs/main/noarch",
            });
            CHECK_EQ(c1.urls(), exp_urls);

            CHECK_EQ(c2.url(), CondaURL::parse("https://repo.anaconda.com/pkgs/r"));
            const UrlSet exp_urls2({
                std::string("https://repo.anaconda.com/pkgs/r/") + platform,
                "https://repo.anaconda.com/pkgs/r/noarch",
            });
            CHECK_EQ(c2.urls(), exp_urls2);

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
            // Hard coded Anaconda multi channel names set by configuration after refactor
            // FIXME: this has side effect on all tests
            ctx.custom_multichannels["defaults"] = ctx.default_channels;
            ChannelContext channel_context{ ctx };

            auto x = channel_context.get_channels({ "defaults" });
            const Channel c1 = x[0];
            const Channel c2 = x[1];

            CHECK_EQ(c1.url(), CondaURL::parse("https://mamba.com/test/channel"));
            const UrlSet exp_urls({
                std::string("https://mamba.com/test/channel/") + platform,
                "https://mamba.com/test/channel/noarch",
            });
            CHECK_EQ(c1.urls(), exp_urls);

            CHECK_EQ(c2.url(), CondaURL::parse("https://mamba.com/stable/channel"));
            const UrlSet exp_urls2({
                std::string("https://mamba.com/stable/channel/") + platform,
                "https://mamba.com/stable/channel/noarch",
            });
            CHECK_EQ(c2.urls(), exp_urls2);

            ctx.custom_channels.clear();
        }

        TEST_CASE("custom_local_channels")
        {
            auto& ctx = mambatests::context();

            // Hard coded Anaconda multi channel names set in configuration after refactor
            // Should be moved to test_config
            // FIXME: this has side effect on all tests
            ctx.custom_multichannels["local"] = std::vector<std::string>{
                ctx.prefix_params.target_prefix / "conda-bld",
                ctx.prefix_params.root_prefix / "conda-bld",
                util::user_home_dir() / "conda-bld",
            };
            ChannelContext channel_context{ ctx };

            CHECK_EQ(channel_context.get_custom_multichannels().at("local").size(), 3);

            auto local_channels = channel_context.get_channels({ "local" });
            CHECK_EQ(local_channels.size(), 3);
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
                CHECK_EQ(c.url(), CondaURL::parse("https://server.com/private/channels/test_channel"));
                CHECK_EQ(c.display_name(), "test_channel");
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
                CHECK_EQ(
                    c.url(),
                    CondaURL::parse("https://server.com/private/channels/test_channel/mylabel/xyz")
                );
                CHECK_EQ(c.display_name(), "test_channel/mylabel/xyz");
                CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));
                const UrlSet exp_urls({
                    std::string("https://server.com/private/channels/test_channel/mylabel/xyz/")
                        + platform,
                    "https://server.com/private/channels/test_channel/mylabel/xyz/noarch",
                });
                CHECK_EQ(c.urls(), exp_urls);
            }

            {
                // https://github.com/mamba-org/mamba/issues/2553
                std::string value = "random/test_channel/pkg";
                const Channel& c = channel_context.make_channel(value);
                CHECK_EQ(
                    c.url(),
                    CondaURL::parse("https://server.com/random/channels/random/test_channel/pkg")
                );
                CHECK_EQ(c.display_name(), "random/test_channel/pkg");
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
            CHECK_EQ(c.url(), CondaURL::parse("https://repo.mamba.pm/conda-forge"));
            CHECK_EQ(c.display_name(), "https://repo.mamba.pm/conda-forge");
            CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));
        }

        TEST_CASE("make_channel")
        {
            std::string value = "conda-forge";
            ChannelContext channel_context{ mambatests::context() };
            const Channel& c = channel_context.make_channel(value);
            CHECK_EQ(c.url(), CondaURL::parse("https://conda.anaconda.org/conda-forge"));
            CHECK_EQ(c.display_name(), "conda-forge");
            CHECK_EQ(c.platforms(), PlatformSet({ platform, "noarch" }));

            std::string value2 = "https://repo.anaconda.com/pkgs/main[" + platform + "]";
            const Channel& c2 = channel_context.make_channel(value2);
            CHECK_EQ(c2.url(), CondaURL::parse("https://repo.anaconda.com/pkgs/main"));
            CHECK_EQ(c2.display_name(), "https://repo.anaconda.com/pkgs/main");
            CHECK_EQ(c2.platforms(), PlatformSet({ platform }));

            std::string value3 = "https://conda.anaconda.org/conda-forge[" + platform + "]";
            const Channel& c3 = channel_context.make_channel(value3);
            CHECK_EQ(c3.url(), c.url());
            CHECK_EQ(c3.display_name(), c.display_name());
            CHECK_EQ(c3.platforms(), PlatformSet({ platform }));

            std::string value4 = "/home/mamba/test/channel_b";
            const Channel& c4 = channel_context.make_channel(value4);
            CHECK_EQ(c4.url(), CondaURL::parse(util::path_to_url(value4)));
            CHECK_EQ(c4.display_name(), c4.url().pretty_str());
            CHECK_EQ(c4.platforms(), PlatformSet({ platform, "noarch" }));

            std::string path5 = "/home/mamba/test/channel_b";
            std::string value5 = util::concat(path5, '[', platform, ']');
            const Channel& c5 = channel_context.make_channel(value5);
            CHECK_EQ(c5.url(), CondaURL::parse(util::path_to_url(path5)));
            CHECK_EQ(c5.display_name(), c5.url().pretty_str());
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
            ctx.authentication_info().emplace(
                "conda.anaconda.org",
                specs::CondaToken{ "my-12345-token" }
            );

            ChannelContext channel_context{ ctx };

            const auto& chan = channel_context.make_channel("conda-forge[noarch]");
            CHECK_EQ(chan.url().token(), "my-12345-token");
            CHECK_EQ(
                chan.urls(true),
                UrlSet({ "https://conda.anaconda.org/t/my-12345-token/conda-forge/noarch" })
            );
            CHECK_EQ(chan.urls(false), UrlSet({ "https://conda.anaconda.org/conda-forge/noarch" }));
        }

        TEST_CASE("add_multiple_tokens")
        {
            auto& ctx = mambatests::context();
            ctx.authentication_info().emplace("conda.anaconda.org", specs::CondaToken{ "base-token" });
            ctx.authentication_info().emplace(
                "conda.anaconda.org/conda-forge",
                specs::CondaToken{ "channel-token" }
            );

            ChannelContext channel_context{ ctx };

            const auto& chan = channel_context.make_channel("conda-forge[noarch]");
            CHECK_EQ(chan.url().token(), "channel-token");
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
            CHECK_EQ(c.base_url(), "http://localhost:8000/");
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
