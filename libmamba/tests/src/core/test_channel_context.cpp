// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/flat_set.hpp"
#include "mamba/util/url_manip.hpp"

#include "doctest-printer/conda_url.hpp"
#include "doctest-printer/flat_set.hpp"

#include "mambatests.hpp"

namespace
{
    using namespace mamba;

    static const std::string platform = std::string(specs::build_platform_name());
    using PlatformSet = typename util::flat_set<std::string>;
    using UrlSet = typename util::flat_set<std::string>;
    using CondaURL = specs::CondaURL;
    using Channel = specs::Channel;

    TEST_CASE("make_conda_compatible default")
    {
        auto ctx = Context();
        auto chan_ctx = ChannelContext::make_conda_compatible(ctx);

        SECTION("Channel alias")
        {
            REQUIRE(chan_ctx.params().channel_alias.str() == "https://conda.anaconda.org/");
        }

        SECTION("Conda pkgs channels")
        {
            const auto& custom = chan_ctx.params().custom_channels;

            const auto& main = custom.at("pkgs/main");
            REQUIRE(main.url() == CondaURL::parse("https://repo.anaconda.com/pkgs/main");
            REQUIRE(main.display_name() == "pkgs/main");

            const auto& pro = custom.at("pkgs/pro");
            REQUIRE(pro.url() == CondaURL::parse("https://repo.anaconda.com/pkgs/pro");
            REQUIRE(pro.display_name() == "pkgs/pro");

            const auto& r = custom.at("pkgs/r");
            REQUIRE(r.url() == CondaURL::parse("https://repo.anaconda.com/pkgs/r");
            REQUIRE(r.display_name() == "pkgs/r");
        }

        SECTION("Defaults")
        {
            const auto& defaults = chan_ctx.params().custom_multichannels.at("defaults");

            auto found_names = util::flat_set<std::string>();
            auto found_urls = util::flat_set<std::string>();
            for (const auto& chan : defaults)
            {
                found_names.insert(chan.display_name());
                found_urls.insert(chan.url().str());
            }
            if (util::on_win)
            {
                REQUIRE(
                    found_names == util::flat_set<std::string>{ "pkgs/main", "pkgs/r", "pkgs/msys2" }
                );
                REQUIRE(
                    found_urls ==
                    util::flat_set<std::string>{
                        "https://repo.anaconda.com/pkgs/main",
                        "https://repo.anaconda.com/pkgs/r",
                        "https://repo.anaconda.com/pkgs/msys2",
                    }
                );
            }
            else
            {
                REQUIRE(found_names == util::flat_set<std::string>{ "pkgs/main", "pkgs/r" });
                REQUIRE(
                    found_urls ==
                    util::flat_set<std::string>{
                        "https://repo.anaconda.com/pkgs/main",
                        "https://repo.anaconda.com/pkgs/r",
                    }
                );
            }
        }

        SECTION("Has zst")
        {
            const auto& chans = chan_ctx.make_channel("https://conda.anaconda.org/conda-forge");
            REQUIRE(chans.size() == 1);
            REQUIRE(chan_ctx.has_zst(chans.at(0)));
        }
    }

    TEST_CASE("make_conda_compatible override")
    {
        auto ctx = Context();

        SECTION("Channel alias override")
        {
            ctx.channel_alias = "https://ali.as";
            auto chan_ctx = ChannelContext::make_conda_compatible(ctx);
            REQUIRE(chan_ctx.params().channel_alias.str() == "https://ali.as/");
        }

        SECTION("Custom channels")
        {
            ctx.custom_channels = {
                { "chan1", "https://repo.mamba.pm/chan1" },
                { "chan2", "https://repo.mamba.pm/" },
                { "pkgs/main", "https://repo.mamba.pm/pkgs/main" },
            };
            auto chan_ctx = ChannelContext::make_conda_compatible(ctx);
            const auto& custom = chan_ctx.params().custom_channels;

            const auto& chan1 = custom.at("chan1");
            REQUIRE(chan1.url() == CondaURL::parse("https://repo.mamba.pm/chan1");
            REQUIRE(chan1.display_name() == "chan1");

            // Conda behaviour that URL ending must match name
            const auto& chan2 = custom.at("chan2");
            REQUIRE(chan2.url() == CondaURL::parse("https://repo.mamba.pm/chan2");
            REQUIRE(chan2.display_name() == "chan2");

            // Explicit override
            const auto& main = custom.at("pkgs/main");
            REQUIRE(main.url() == CondaURL::parse("https://repo.mamba.pm/pkgs/main");
            REQUIRE(main.display_name() == "pkgs/main");
        }

        SECTION("Custom defaults")
        {
            ctx.default_channels = {
                "https://mamba.com/test/channel",
                "https://mamba.com/stable/channel",
            };
            auto chan_ctx = ChannelContext::make_conda_compatible(ctx);
            const auto& defaults = chan_ctx.params().custom_multichannels.at("defaults");

            auto found_urls = util::flat_set<std::string>();
            for (const auto& chan : defaults)
            {
                found_urls.insert(chan.url().str());
            }
            REQUIRE(
                found_urls ==
                util::flat_set<std::string>{
                    "https://mamba.com/test/channel",
                    "https://mamba.com/stable/channel",
                }
            );
        }

        SECTION("Local")
        {
            const auto tmp_dir = TemporaryDirectory();
            const auto conda_bld = tmp_dir.path() / "conda-bld";
            fs::create_directory(conda_bld);

            SECTION("HOME")
            {
                const auto restore = mambatests::EnvironmentCleaner();
                util::set_env("HOME", tmp_dir.path());         // Unix
                util::set_env("USERPROFILE", tmp_dir.path());  // Win

                auto chan_ctx = ChannelContext::make_conda_compatible(ctx);
                const auto& local = chan_ctx.params().custom_multichannels.at("local");

                REQUIRE(local.size() == 1);
                REQUIRE(local.front().url() == CondaURL::parse(util::path_to_url(conda_bld.string())));
            }

            SECTION("Root prefix")
            {
                ctx.prefix_params.root_prefix = tmp_dir.path();
                auto chan_ctx = ChannelContext::make_conda_compatible(ctx);
                const auto& local = chan_ctx.params().custom_multichannels.at("local");

                REQUIRE(local.size() == 1);
                REQUIRE(local.front().url() == CondaURL::parse(util::path_to_url(conda_bld.string())));
            }

            SECTION("Target prefix")
            {
                ctx.prefix_params.root_prefix = tmp_dir.path();
                auto chan_ctx = ChannelContext::make_conda_compatible(ctx);
                const auto& local = chan_ctx.params().custom_multichannels.at("local");

                REQUIRE(local.size() == 1);
                REQUIRE(local.front().url() == CondaURL::parse(util::path_to_url(conda_bld.string())));
            }
        }

        SECTION("Custom multi channels")
        {
            ctx.channel_alias = "https://ali.as";
            ctx.custom_multichannels["mymulti"] = std::vector<std::string>{
                "conda-forge",
                "https://mydomain.com/bioconda",
                "https://mydomain.com/snakepit",
            };
            ctx.custom_multichannels["defaults"] = std::vector<std::string>{
                "https://otherdomain.com/conda-forge",
                "bioconda",
                "https://otherdomain.com/snakepit",
            };
            auto chan_ctx = ChannelContext::make_conda_compatible(ctx);

            // mymulti
            {
                const auto& mymulti = chan_ctx.params().custom_multichannels.at("mymulti");

                auto found_names = util::flat_set<std::string>();
                auto found_urls = util::flat_set<std::string>();
                for (const auto& chan : mymulti)
                {
                    found_names.insert(chan.display_name());
                    found_urls.insert(chan.url().str());
                }
                REQUIRE(
                    found_names ==
                    util::flat_set<std::string>{
                        "conda-forge",
                        "https://mydomain.com/bioconda",
                        "https://mydomain.com/snakepit",
                    }
                );
                REQUIRE(
                    found_urls ==
                    util::flat_set<std::string>{
                        "https://ali.as/conda-forge",
                        "https://mydomain.com/bioconda",
                        "https://mydomain.com/snakepit",
                    }
                );
            }

            // Explicitly override defaults
            {
                const auto& defaults = chan_ctx.params().custom_multichannels.at("defaults");

                auto found_names = util::flat_set<std::string>();
                auto found_urls = util::flat_set<std::string>();
                for (const auto& chan : defaults)
                {
                    found_names.insert(chan.display_name());
                    found_urls.insert(chan.url().str());
                }
                REQUIRE(
                    found_names ==
                    util::flat_set<std::string>{
                        "https://otherdomain.com/conda-forge",
                        "bioconda",
                        "https://otherdomain.com/snakepit",
                    }
                );
                REQUIRE(
                    found_urls ==
                    util::flat_set<std::string>{
                        "https://otherdomain.com/conda-forge",
                        "https://ali.as/bioconda",
                        "https://otherdomain.com/snakepit",
                    }
                );
            }
        }
    }

    TEST_CASE("make_simple")
    {
        auto ctx = Context();

        SECTION("Channel alias")
        {
            ctx.channel_alias = "https://ali.as";
            auto chan_ctx = ChannelContext::make_simple(ctx);
            REQUIRE(chan_ctx.params().channel_alias.str() == "https://ali.as/");
        }

        SECTION("Custom channels")
        {
            ctx.custom_channels = {
                { "chan1", "https://repo.mamba.pm/chan1" },
                { "chan2", "https://repo.mamba.pm/" },
                { "pkgs/main", "https://repo.mamba.pm/pkgs/main" },
            };
            auto chan_ctx = ChannelContext::make_simple(ctx);
            const auto& custom = chan_ctx.params().custom_channels;

            const auto& chan1 = custom.at("chan1");
            REQUIRE(chan1.url() == CondaURL::parse("https://repo.mamba.pm/chan1");
            REQUIRE(chan1.display_name() == "chan1");

            // Different from Conda behaviour
            const auto& chan2 = custom.at("chan2");
            REQUIRE(chan2.url() == CondaURL::parse("https://repo.mamba.pm/");
            REQUIRE(chan2.display_name() == "chan2");

            // Explicitly created
            const auto& main = custom.at("pkgs/main");
            REQUIRE(main.url() == CondaURL::parse("https://repo.mamba.pm/pkgs/main");
            REQUIRE(main.display_name() == "pkgs/main");
        }

        SECTION("No hard coded names")
        {
            auto chan_ctx = ChannelContext::make_simple(ctx);

            const auto& custom = chan_ctx.params().custom_multichannels;
            REQUIRE(custom.find("pkgs/main") == custom.cend();
            REQUIRE(custom.find("pkgs/r") == custom.cend();
            REQUIRE(custom.find("pkgs/pro") == custom.cend();
            REQUIRE(custom.find("pkgs/msys2") == custom.cend();

            const auto& custom_multi = chan_ctx.params().custom_multichannels;
            REQUIRE(custom_multi.find("defaults") == custom_multi.cend();
            REQUIRE(custom_multi.find("local") == custom_multi.cend();
        }

        SECTION("Custom multi channels")
        {
            ctx.channel_alias = "https://ali.as";
            ctx.custom_multichannels["mymulti"] = std::vector<std::string>{
                "conda-forge",
                "https://mydomain.com/bioconda",
                "https://mydomain.com/snakepit",
            };
            ctx.custom_multichannels["defaults"] = std::vector<std::string>{
                "https://otherdomain.com/conda-forge",
                "bioconda",
                "https://otherdomain.com/snakepit",
            };
            auto chan_ctx = ChannelContext::make_simple(ctx);

            // mymulti
            {
                const auto& mymulti = chan_ctx.params().custom_multichannels.at("mymulti");

                auto found_names = util::flat_set<std::string>();
                auto found_urls = util::flat_set<std::string>();
                for (const auto& chan : mymulti)
                {
                    found_names.insert(chan.display_name());
                    found_urls.insert(chan.url().str());
                }
                REQUIRE(
                    found_names ==
                    util::flat_set<std::string>{
                        "conda-forge",
                        "https://mydomain.com/bioconda",
                        "https://mydomain.com/snakepit",
                    }
                );
                REQUIRE(
                    found_urls ==
                    util::flat_set<std::string>{
                        "https://ali.as/conda-forge",
                        "https://mydomain.com/bioconda",
                        "https://mydomain.com/snakepit",
                    }
                );
            }

            // Explicitly created defaults
            {
                const auto& defaults = chan_ctx.params().custom_multichannels.at("defaults");

                auto found_names = util::flat_set<std::string>();
                auto found_urls = util::flat_set<std::string>();
                for (const auto& chan : defaults)
                {
                    found_names.insert(chan.display_name());
                    found_urls.insert(chan.url().str());
                }
                REQUIRE(
                    found_names ==
                    util::flat_set<std::string>{
                        "https://otherdomain.com/conda-forge",
                        "bioconda",
                        "https://otherdomain.com/snakepit",
                    }
                );
                REQUIRE(
                    found_urls ==
                    util::flat_set<std::string>{
                        "https://otherdomain.com/conda-forge",
                        "https://ali.as/bioconda",
                        "https://otherdomain.com/snakepit",
                    }
                );
            }
        }

        SECTION("Has zst")
        {
            ctx.repodata_has_zst = { "https://otherdomain.com/conda-forge[noarch,linux-64]" };

            SECTION("enabled")
            {
                ctx.repodata_use_zst = true;
                auto chan_ctx = ChannelContext::make_simple(ctx);

                {
                    const auto& chans = chan_ctx.make_channel(
                        "https://otherdomain.com/conda-forge[noarch]"
                    );
                    REQUIRE(chans.size() == 1);
                    REQUIRE(chan_ctx.has_zst(chans.at(0)));
                }
                {
                    const auto& chans = chan_ctx.make_channel(
                        "https://otherdomain.com/conda-forge[win-64]"
                    );
                    REQUIRE(chans.size() == 1);
                    REQUIRE_FALSE(chan_ctx.has_zst(chans.at(0)));
                }
                {
                    const auto& chans = chan_ctx.make_channel("https://conda.anaconda.org/conda-forge"
                    );
                    REQUIRE(chans.size() == 1);
                    REQUIRE_FALSE(chan_ctx.has_zst(chans.at(0)));
                }
            }

            SECTION("disabled")
            {
                ctx.repodata_use_zst = false;
                auto chan_ctx = ChannelContext::make_simple(ctx);

                const auto& chans = chan_ctx.make_channel("https://otherdomain.com/conda-forge");
                REQUIRE(chans.size() == 1);
                REQUIRE_FALSE(chan_ctx.has_zst(chans.at(0)));
            }
        }
    }
}
