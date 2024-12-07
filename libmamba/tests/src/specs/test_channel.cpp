// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/channel.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

#include "catch-utils/conda_url.hpp"

using namespace mamba;
using namespace mamba::specs;
using platform_list = Channel::platform_list;
using namespace std::literals::string_view_literals;

TEST_CASE("Channel")
{
    SECTION("Constructor railing slash")
    {
        // Leading slash for empty paths
        for (auto url : {
                 "https://repo.mamba.pm/"sv,
                 "https://repo.mamba.pm"sv,
             })
        {
            CAPTURE(url);
            auto chan = Channel(CondaURL::parse(url).value(), "somename");
            REQUIRE(chan.url().str() != mamba::util::rstrip(url, '/'));
        }

        // No trailing slash for paths
        for (auto url : {
                 "https://repo.mamba.pm/conda-forge/win-64/"sv,
                 "file:///some/folder/"sv,
                 "ftp://mamba.org/some/folder"sv,
             })
        {
            CAPTURE(url);
            auto chan = Channel(CondaURL::parse(url).value(), "somename");
            REQUIRE(chan.url().str() == mamba::util::rstrip(url, '/'));
        }
    }

    SECTION("Equality")
    {
        for (auto raw_url : {
                 "https://repo.mamba.pm/"sv,
                 "https://repo.mamba.pm"sv,
                 "https://repo.mamba.pm/conda-forge/win-64/"sv,
                 "file:///some/folder/"sv,
                 "ftp://mamba.org/some/folder"sv,
             })
        {
            CAPTURE(raw_url);

            auto chan_a = Channel(CondaURL::parse(raw_url).value(), "somename", { "linux-64" });
            REQUIRE(chan_a == chan_a);

            auto chan_b = chan_a;
            REQUIRE(chan_b == chan_a);
            REQUIRE(chan_a == chan_b);

            chan_b = Channel(chan_a.url(), chan_a.display_name(), { "linux-64", "noarch" });
            REQUIRE(chan_b != chan_a);

            chan_b = Channel(chan_a.url(), "othername", chan_a.platforms());
            REQUIRE(chan_b != chan_a);
        }
    }

    SECTION("Equivalence")
    {
        SECTION("Same platforms")
        {
            for (auto raw_url : {
                     "https://repo.mamba.pm/"sv,
                     "https://repo.mamba.pm/t/mytoken/"sv,
                     "https://user:pass@repo.mamba.pm/conda-forge/"sv,
                     "file:///some/folder/"sv,
                     "ftp://mamba.org/some/folder"sv,
                 })
            {
                CAPTURE(raw_url);

                auto url_a = CondaURL::parse(raw_url).value();
                auto url_b = url_a;
                url_b.clear_user();
                url_b.clear_password();
                url_b.clear_token();
                auto chan_a = Channel(url_a, "somename", { "linux-64" });
                auto chan_b = Channel(url_b, "somename", { "linux-64" });

                // Channel::url_equivalent_with
                REQUIRE(chan_a.url_equivalent_with(chan_a));
                REQUIRE(chan_b.url_equivalent_with(chan_b));
                REQUIRE(chan_a.url_equivalent_with(chan_b));
                REQUIRE(chan_b.url_equivalent_with(chan_a));

                // Channel::contains_equivalent
                REQUIRE(chan_a.contains_equivalent(chan_a));
                REQUIRE(chan_b.contains_equivalent(chan_b));
                REQUIRE(chan_a.contains_equivalent(chan_b));
                REQUIRE(chan_b.contains_equivalent(chan_a));
            }
        }

        SECTION("Platforms superset")
        {
            for (auto raw_url : {
                     "https://repo.mamba.pm/"sv,
                     "https://repo.mamba.pm/t/mytoken/"sv,
                     "https://user:pass@repo.mamba.pm/conda-forge/"sv,
                     "file:///some/folder/"sv,
                     "ftp://mamba.org/some/folder"sv,
                 })
            {
                CAPTURE(raw_url);

                auto url_a = CondaURL::parse(raw_url).value();
                auto url_b = url_a;
                url_a.clear_user();
                url_a.clear_password();
                url_a.clear_token();
                auto chan_a = Channel(url_a, "somename", { "noarch", "linux-64" });
                auto chan_b = Channel(url_b, "somename", { "linux-64" });

                REQUIRE(chan_a.contains_equivalent(chan_a));
                REQUIRE(chan_a.contains_equivalent(chan_b));
                REQUIRE_FALSE(chan_b.contains_equivalent(chan_a));
            }
        }

        SECTION("Different platforms")
        {
            for (auto raw_url : {
                     "https://repo.mamba.pm/"sv,
                     "https://repo.mamba.pm/t/mytoken/"sv,
                     "https://user:pass@repo.mamba.pm/conda-forge/"sv,
                     "file:///some/folder/"sv,
                     "ftp://mamba.org/some/folder"sv,
                 })
            {
                CAPTURE(raw_url);

                auto url_a = CondaURL::parse(raw_url).value();
                auto url_b = url_a;
                auto chan_a = Channel(url_a, "somename", { "noarch", "linux-64" });
                auto chan_b = Channel(url_b, "somename", { "osx-64" });

                REQUIRE_FALSE(chan_a.contains_equivalent(chan_b));
                REQUIRE_FALSE(chan_b.contains_equivalent(chan_a));
            }
        }

        SECTION("Packages")
        {
            using namespace conda_url_literals;

            const auto chan = Channel("https://repo.mamba.pm/"_cu, "conda-forge", { "linux-64" });
            REQUIRE(chan.contains_equivalent(Channel(chan.url() / "linux-64/pkg.conda", "", {})));
            REQUIRE_FALSE(chan.contains_equivalent(Channel(chan.url() / "osx-64/pkg.conda", "", {})));

            const auto pkg_chan = Channel(chan.url() / "linux-64/foo.tar.bz2", "", {});
            REQUIRE(pkg_chan.contains_equivalent(pkg_chan));
            REQUIRE_FALSE(pkg_chan.contains_equivalent(chan));
            REQUIRE_FALSE(
                pkg_chan.contains_equivalent(Channel(chan.url() / "osx-64/pkg.conda", "", {}))
            );
        }
    }

    SECTION("Contains package")
    {
        using namespace conda_url_literals;
        using Match = Channel::Match;

        SECTION("https://repo.mamba.pm/")
        {
            auto chan = Channel("https://repo.mamba.pm/"_cu, "conda-forge", { "linux-64" });
            REQUIRE(
                chan.contains_package("https://repo.mamba.pm/linux-64/pkg.conda"_cu) == Match::Full
            );
            REQUIRE(
                chan.contains_package("https://repo.mamba.pm/win-64/pkg.conda"_cu)
                == Match::InOtherPlatform
            );
            REQUIRE(
                chan.contains_package("https://repo.mamba.pm/pkg.conda"_cu) == Match::InOtherPlatform
            );
        }

        SECTION("https://repo.mamba.pm/osx-64/foo.tar.gz")
        {
            auto chan = Channel("https://repo.mamba.pm/osx-64/foo.tar.bz2"_cu, "", {});
            REQUIRE(chan.contains_package(chan.url()) == Match::Full);
            REQUIRE(chan.contains_package("https://repo.mamba.pm/win-64/pkg.conda"_cu) == Match::No);
            REQUIRE(chan.contains_package("https://repo.mamba.pm/pkg.conda"_cu) == Match::No);
        }

        SECTION("https://user:pass@repo.mamba.pm/conda-forge/")
        {
            auto chan = Channel(
                "https://user:pass@repo.mamba.pm/conda-forge/"_cu,
                "conda-forge",
                { "win-64" }
            );
            REQUIRE(chan.contains_package(chan.url() / "win-64/pkg.conda") == Match::Full);
            REQUIRE(
                chan.contains_package("https://repo.mamba.pm/conda-forge/win-64/pkg.conda"_cu)
                == Match::Full
            );
            REQUIRE(
                chan.contains_package("https://repo.mamba.pm/conda-forge/osx-64/pkg.conda"_cu)
                == Match::InOtherPlatform
            );
        }
    }
}

TEST_CASE("Channel::resolve")
{
    auto make_typical_params = []() -> ChannelResolveParams
    {
        auto make_channel = [](std::string_view loc, const ChannelResolveParams& params)
        { return Channel::resolve(UnresolvedChannel::parse(loc).value(), params).value().at(0); };

        auto params = ChannelResolveParams{};
        params.platforms = { "linux-64", "noarch" };
        params.home_dir = "/home";
        params.current_working_dir = "/cwd";
        params.channel_alias = CondaURL::parse("https://conda.anaconda.org/").value();
        params.custom_channels = {
            {
                { "pkgs/main", make_channel("https://repo.anaconda.com/pkgs/main", params) },
                { "pkgs/r", make_channel("https://repo.anaconda.com/pkgs/r", params) },
                { "pkgs/pro", make_channel("https://repo.anaconda.com/pkgs/pro", params) },
            },
        };
        params.custom_multichannels = {
            {
                "defaults",
                {
                    make_channel("pkgs/main", params),
                    make_channel("pkgs/r", params),
                    make_channel("pkgs/pro", params),
                },
            },
            { "local", { make_channel("~/conda-bld", params) } },
        };
        return params;
    };

    SECTION("/path/to/libmamba-1.4.2-hcea66bb_0.conda")
    {
        const auto path = "/path/to/libmamba-1.4.2-hcea66bb_0.conda"sv;
        auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::PackagePath);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            const auto url = "file:///path/to/libmamba-1.4.2-hcea66bb_0.conda"sv;
            REQUIRE(chan.url() == CondaURL::parse(url).value());
            REQUIRE(chan.platforms() == platform_list());  // Empty because package
            REQUIRE(chan.display_name() == url);
        }
    }

    SECTION("~/conda-bld/win-64/libmamba-1.4.2-hcea66bb_0.conda")
    {
        const auto path = "~/conda-bld/win-64/libmamba-1.4.2-hcea66bb_0.conda"sv;
        auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::PackagePath);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            const auto url = "file:///home/conda-bld/win-64/libmamba-1.4.2-hcea66bb_0.conda"sv;
            REQUIRE(chan.url() == CondaURL::parse(url).value());
            REQUIRE(chan.platforms() == platform_list());  // Empty because package
            REQUIRE(chan.display_name() == url);
        }

        SECTION("Matching channel alias")
        {
            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ CondaURL::parse("file:///home/conda-bld").value(),
                /* .custom_channels= */ {},
                /* .custom_multichannels= */ {},
                /* .authentication_db= */ {},
                /* .home_dir= */ "/home",
            };
            REQUIRE(
                Channel::resolve(uc, params).value().at(0).display_name()
                == "win-64/libmamba-1.4.2-hcea66bb_0.conda"
            );
        }

        SECTION("Custom channel")
        {
            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ CondaURL::parse("file:///home/conda-bld").value(),
                /* .custom_channels= */ {},
                /* .custom_multichannels= */ {},
                /* .authentication_db= */ {},
                /* .home_dir= */ "/home",
            };
            params.custom_channels.emplace(
                "mychan",
                Channel::resolve(UnresolvedChannel::parse("file:///home/conda-bld/").value(), params)
                    .value()
                    .at(0)
            );
            REQUIRE(Channel::resolve(uc, params).value().at(0).display_name() == "mychan");
        }
    }

    SECTION("./path/to/libmamba-1.4.2-hcea66bb_0.conda")
    {
        const auto path = "./path/to/libmamba-1.4.2-hcea66bb_0.conda"sv;
        auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::PackagePath);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            const auto url = "file:///cwd/path/to/libmamba-1.4.2-hcea66bb_0.conda"sv;
            REQUIRE(chan.url() == CondaURL::parse(url).value());
            REQUIRE(chan.platforms() == platform_list());  // Empty because package
            REQUIRE(chan.display_name() == url);
        }
    }

    SECTION("/some/folder")
    {
        const auto path = "/some/folder"sv;
        auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::Path);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            const auto url = "file:///some/folder"sv;
            REQUIRE(chan.url() == CondaURL::parse(url).value());
            REQUIRE(chan.platforms() == params.platforms);
            REQUIRE(chan.display_name() == url);
        }

        SECTION("With platform filers")
        {
            auto other_specs = UnresolvedChannel(
                std::string(path),
                { "foo-56" },
                UnresolvedChannel::Type::Path
            );
            REQUIRE(
                Channel::resolve(other_specs, ChannelResolveParams{}).value().at(0).platforms()
                == other_specs.platform_filters()
            );
        }
    }

    SECTION("~/folder")
    {
        const auto path = "~/folder"sv;
        auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::Path);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            const auto url = "file:///home/folder"sv;
            REQUIRE(chan.url() == CondaURL::parse(url).value());
            REQUIRE(chan.platforms() == params.platforms);
            REQUIRE(chan.display_name() == url);
        }
    }

    SECTION("./other/folder")
    {
        const auto path = "./other/folder"sv;
        auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::Path);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            const auto url = "file:///cwd/other/folder"sv;
            REQUIRE(chan.url() == CondaURL::parse(url));
            REQUIRE(chan.platforms() == params.platforms);
            REQUIRE(chan.display_name() == url);
        }
    }

    SECTION("https://repo.mamba.pm/conda-forge/linux-64/libmamba-1.4.2-hcea66bb_0.conda")
    {
        const auto url = "https://repo.mamba.pm/conda-forge/linux-64/libmamba-1.4.2-hcea66bb_0.conda"sv;
        auto uc = UnresolvedChannel(std::string(url), {}, UnresolvedChannel::Type::PackageURL);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(url));
            REQUIRE(chan.platforms() == platform_list());  // Empty because package
            REQUIRE(chan.display_name() == url);
        }
    }

    SECTION("https://repo.mamba.pm")
    {
        const auto url = "https://repo.mamba.pm"sv;
        auto uc = UnresolvedChannel(
            std::string(url),
            { "linux-64", "noarch" },
            UnresolvedChannel::Type::URL
        );

        SECTION("Empty params")
        {
            auto channels = Channel::resolve(uc, ChannelResolveParams{}).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(url));
            REQUIRE(chan.platforms() == uc.platform_filters());
            REQUIRE(chan.display_name() == url);
        }

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(url));
            REQUIRE(chan.platforms() == uc.platform_filters());
            REQUIRE(chan.display_name() == url);
        }
    }

    SECTION("https://repo.mamba.pm/conda-forge")
    {
        const auto url = "https://repo.mamba.pm/conda-forge"sv;
        auto uc = UnresolvedChannel(
            std::string(url),
            { "linux-64", "noarch" },
            UnresolvedChannel::Type::URL
        );

        SECTION("Empty params")
        {
            auto channels = Channel::resolve(uc, ChannelResolveParams{}).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(url));
            REQUIRE(chan.platforms() == uc.platform_filters());
            REQUIRE(chan.display_name() == url);
        }

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(url));
            REQUIRE(chan.platforms() == uc.platform_filters());
            REQUIRE(chan.display_name() == url);
        }

        SECTION("Default platforms")
        {
            auto params = ChannelResolveParams{ /* .platform= */ { "rainbow-37", "noarch" } };
            REQUIRE(Channel::resolve(uc, params).value().at(0).platforms() == uc.platform_filters());

            uc.clear_platform_filters();
            REQUIRE(Channel::resolve(uc, params).value().at(0).platforms() == params.platforms);
        }

        SECTION("Matching channel alias")
        {
            for (auto alias : {
                     "https://repo.mamba.pm/"sv,
                     "https://repo.mamba.pm"sv,
                     "repo.mamba.pm"sv,
                 })
            {
                CAPTURE(alias);
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ CondaURL::parse(alias).value(),
                };
                REQUIRE(Channel::resolve(uc, params).value().at(0).display_name() == "conda-forge");
            }
        }

        SECTION("Not matching channel alias")
        {
            for (auto alias : {
                     "repo.anaconda.com"sv,
                     "ftp://repo.mamba.pm"sv,
                 })
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ CondaURL::parse(alias).value(),
                };
                REQUIRE(Channel::resolve(uc, params).value().at(0).display_name() == url);
            }
        }

        SECTION("Custom channel")
        {
            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ CondaURL::parse("https://repo.mamba.pm/").value(),
                /* .custom_channels= */
                { { "mychan", Channel::resolve(uc, ChannelResolveParams{}).value().at(0) } }
            };
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(url).value());
            REQUIRE(chan.display_name() == "mychan");
        }

        SECTION("Authentication info")
        {
            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ {},
                /* .custom_channels= */ {},
                /* .custom_multichannels= */ {},
                /* .authentication_db= */ { { "repo.mamba.pm", CondaToken{ "mytoken" } } },
            };

            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(
                chan.url() == CondaURL::parse("https://repo.mamba.pm/t/mytoken/conda-forge").value()
            );
            REQUIRE(chan.display_name() == "https://repo.mamba.pm/conda-forge");
        }

        SECTION("Authentication info multiple tokens")
        {
            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ {},
                /* .custom_channels= */ {},
                /* .custom_multichannels= */ {},
                /* .authentication_db= */
                {
                    { "repo.mamba.pm", CondaToken{ "mytoken" } },
                    { "repo.mamba.pm/conda-forge", CondaToken{ "forge-token" } },
                },
            };

            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(
                chan.url()
                == CondaURL::parse("https://repo.mamba.pm/t/forge-token/conda-forge").value()
            );
            REQUIRE(chan.display_name() == "https://repo.mamba.pm/conda-forge");
        }
    }

    SECTION("https://user:pass@repo.mamba.pm/conda-forge")
    {
        const auto url = "https://user:pass@repo.mamba.pm/conda-forge"sv;
        auto uc = UnresolvedChannel(std::string(url), {}, UnresolvedChannel::Type::URL);

        SECTION("Authentication info token")
        {
            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ {},
                /* .custom_channels= */ {},
                /* .custom_multichannels= */ {},
                /* .authentication_db= */ { { "repo.mamba.pm", CondaToken{ "mytoken" } } },
            };

            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(
                chan.url() == CondaURL::parse("https://user:pass@repo.mamba.pm/t/mytoken/conda-forge")
            );
            REQUIRE(chan.display_name() == "https://repo.mamba.pm/conda-forge");
        }

        SECTION("Authentication info user password")
        {
            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ {},
                /* .custom_channels= */ {},
                /* .custom_multichannels= */ {},
                /* .authentication_db= */
                { { "repo.mamba.pm", BasicHTTPAuthentication{ "foo", "weak" } } },
            };

            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            // Higher precedence
            REQUIRE(chan.url() == CondaURL::parse("https://user:pass@repo.mamba.pm/conda-forge"));
            REQUIRE(chan.display_name() == "https://repo.mamba.pm/conda-forge");
        }
    }

    SECTION("https://repo.anaconda.com/pkgs/main")
    {
        const auto url = "https://repo.anaconda.com/pkgs/main"sv;
        auto uc = UnresolvedChannel(std::string(url), {}, UnresolvedChannel::Type::URL);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(url));
            REQUIRE(chan.platforms() == params.platforms);
            REQUIRE(chan.display_name() == "pkgs/main");
        }

        SECTION("Matching channel alias")
        {
            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ CondaURL::parse("https://repo.anaconda.com").value(),
            };

            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(url));
            REQUIRE(chan.display_name() == "pkgs/main");
        }
    }

    SECTION("conda-forge")
    {
        const auto name = "conda-forge"sv;
        auto uc = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(
                chan.url()
                == CondaURL::parse(util::path_concat(params.channel_alias.str(), name)).value()
            );
            REQUIRE(chan.platforms() == params.platforms);
            REQUIRE(chan.display_name() == name);
        }

        SECTION("Authentication info user password")
        {
            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ CondaURL::parse("mydomain.com/private").value(),
                /* .custom_channels= */ {},
                /* .custom_multichannels= */ {},
                /* .authentication_db= */
                { { "mydomain.com", BasicHTTPAuthentication{ "user", "pass" } } },
            };

            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(
                chan.url() == CondaURL::parse("https://user:pass@mydomain.com/private/conda-forge")
            );
            REQUIRE(chan.display_name() == name);
            REQUIRE(chan.platforms() == params.platforms);
        }

        SECTION("Custom channel")
        {
            auto params = make_typical_params();
            params.custom_channels.emplace(
                "conda-forge",
                Channel::resolve(UnresolvedChannel::parse("ftp://mydomain.net/conda").value(), params)
                    .value()
                    .at(0)
            );

            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            // Higher precedence.
            REQUIRE(chan.url() == CondaURL::parse("ftp://mydomain.net/conda"));
            REQUIRE(chan.display_name() == name);
            REQUIRE(chan.platforms() == params.platforms);
        }
    }

    SECTION("pkgs/main")
    {
        const auto name = "pkgs/main"sv;
        auto uc = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse("https://repo.anaconda.com/pkgs/main"));
            REQUIRE(chan.platforms() == params.platforms);
            REQUIRE(chan.display_name() == name);
        }
    }

    SECTION("pkgs/main/label/dev")
    {
        const auto name = "pkgs/main/label/dev"sv;
        auto specs = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(specs, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse("https://repo.anaconda.com/pkgs/main/label/dev"));
            REQUIRE(chan.platforms() == params.platforms);
            REQUIRE(chan.display_name() == name);
        }
    }

    SECTION("testchannel/mylabel/xyz")
    {
        const auto name = "testchannel/mylabel/xyz"sv;
        auto uc = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(util::path_concat(params.channel_alias.str(), name)));
            REQUIRE(chan.platforms() == params.platforms);
            REQUIRE(chan.display_name() == name);
        }

        SECTION("Custom channel")
        {
            auto params = make_typical_params();
            params.custom_channels.emplace(
                "testchannel",
                Channel::resolve(
                    UnresolvedChannel::parse("https://server.com/private/testchannel").value(),
                    params
                )
                    .value()
                    .at(0)
            );

            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(
                chan.url() == CondaURL::parse("https://server.com/private/testchannel/mylabel/xyz")
            );
            REQUIRE(chan.display_name() == name);
            REQUIRE(chan.platforms() == params.platforms);
        }
    }

    SECTION("prefix-and-more")
    {
        const auto name = "prefix-and-more"sv;
        auto uc = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

        auto params = ChannelResolveParams{
            /* .platform= */ {},
            /* .channel_alias= */ CondaURL::parse("https://ali.as/").value(),
        };
        params.custom_channels.emplace(
            "prefix",
            Channel::resolve(UnresolvedChannel::parse("https://server.com/prefix").value(), params)
                .value()
                .at(0)
        );

        auto channels = Channel::resolve(uc, params).value();
        REQUIRE(channels.size() == 1);
        const auto& chan = channels.front();
        REQUIRE(chan.url() == CondaURL::parse("https://ali.as/prefix-and-more"));
        REQUIRE(chan.display_name() == name);
        REQUIRE(chan.platforms() == params.platforms);
    }

    SECTION("defaults")
    {
        const auto name = "defaults"sv;
        auto uc = UnresolvedChannel(std::string(name), { "linux-64" }, UnresolvedChannel::Type::Name);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 3);

            auto found_names = util::flat_set<std::string>();
            for (const auto& chan : channels)
            {
                REQUIRE(chan.platforms() == uc.platform_filters());  // Overridden
                found_names.insert(chan.display_name());
            }
            REQUIRE(found_names == util::flat_set<std::string>{ "pkgs/main", "pkgs/pro", "pkgs/r" });
        }
    }

    SECTION("<unknown>")
    {
        auto uc = UnresolvedChannel({}, { "linux-64" }, UnresolvedChannel::Type::Unknown);
        auto channels = Channel::resolve(uc, ChannelResolveParams{}).value();
        REQUIRE(channels.size() == 1);
        const auto& chan = channels.front();
        REQUIRE(chan.url() == CondaURL());
        REQUIRE(chan.platforms() == platform_list());
        REQUIRE(chan.display_name() == "<unknown>");
    }

    SECTION("https://conda.anaconda.org/conda-forge/linux-64/x264-1%21164.3095-h166bdaf_2.tar.bz2")
    {
        // Version 1!164.3095 is URL encoded
        const auto url = "https://conda.anaconda.org/conda-forge/linux-64/x264-1%21164.3095-h166bdaf_2.tar.bz2"sv;
        auto uc = UnresolvedChannel(std::string(url), {}, UnresolvedChannel::Type::PackageURL);

        SECTION("Typical parameters")
        {
            auto params = make_typical_params();
            auto channels = Channel::resolve(uc, params).value();
            REQUIRE(channels.size() == 1);
            const auto& chan = channels.front();
            REQUIRE(chan.url() == CondaURL::parse(url));
            REQUIRE(chan.platforms() == platform_list());  // Empty because package
            REQUIRE(chan.display_name() == "conda-forge/linux-64/x264-1!164.3095-h166bdaf_2.tar.bz2");
        }
    }
}
