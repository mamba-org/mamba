// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/channel.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

#include "doctest-printer/conda_url.hpp"
#include "doctest-printer/flat_set.hpp"

TEST_SUITE("specs::channel")
{
    using namespace mamba;
    using namespace mamba::specs;
    using platform_list = Channel::platform_list;
    using namespace std::literals::string_view_literals;

    TEST_CASE("Channel")
    {
        SUBCASE("Constructor railing slash")
        {
            // Leading slash for empty paths
            for (auto url : {
                     "https://repo.mamba.pm/"sv,
                     "https://repo.mamba.pm"sv,
                 })
            {
                CAPTURE(url);
                auto chan = Channel(CondaURL::parse(url), "somename");
                CHECK_NE(chan.url().str(), mamba::util::rstrip(url, '/'));
            }

            // No trailing slash for paths
            for (auto url : {
                     "https://repo.mamba.pm/conda-forge/win-64/"sv,
                     "file:///some/folder/"sv,
                     "ftp://mamba.org/some/folder"sv,
                 })
            {
                CAPTURE(url);
                auto chan = Channel(CondaURL::parse(url), "somename");
                CHECK_EQ(chan.url().str(), mamba::util::rstrip(url, '/'));
            }
        }

        SUBCASE("Equality")
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

                auto chan_a = Channel(CondaURL::parse(raw_url), "somename", { "linux-64" });
                CHECK_EQ(chan_a, chan_a);

                auto chan_b = chan_a;
                CHECK_EQ(chan_b, chan_a);
                CHECK_EQ(chan_a, chan_b);

                chan_b = Channel(chan_a.url(), chan_a.display_name(), { "linux-64", "noarch" });
                CHECK_NE(chan_b, chan_a);

                chan_b = Channel(chan_a.url(), "othername", chan_a.platforms());
                CHECK_NE(chan_b, chan_a);
            }
        }

        SUBCASE("Equivalence")
        {
            SUBCASE("Same platforms")
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

                    auto url_a = CondaURL::parse(raw_url);
                    auto url_b = url_a;
                    url_b.clear_user();
                    url_b.clear_password();
                    url_b.clear_token();
                    auto chan_a = Channel(url_a, "somename", { "linux-64" });
                    auto chan_b = Channel(url_b, "somename", { "linux-64" });

                    // Channel::url_equivalent_with
                    CHECK(chan_a.url_equivalent_with(chan_a));
                    CHECK(chan_b.url_equivalent_with(chan_b));
                    CHECK(chan_a.url_equivalent_with(chan_b));
                    CHECK(chan_b.url_equivalent_with(chan_a));

                    // Channel::contains_equivalent
                    CHECK(chan_a.contains_equivalent(chan_a));
                    CHECK(chan_b.contains_equivalent(chan_b));
                    CHECK(chan_a.contains_equivalent(chan_b));
                    CHECK(chan_b.contains_equivalent(chan_a));
                }
            }

            SUBCASE("Platforms superset")
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

                    auto url_a = CondaURL::parse(raw_url);
                    auto url_b = url_a;
                    url_a.clear_user();
                    url_a.clear_password();
                    url_a.clear_token();
                    auto chan_a = Channel(url_a, "somename", { "noarch", "linux-64" });
                    auto chan_b = Channel(url_b, "somename", { "linux-64" });

                    CHECK(chan_a.contains_equivalent(chan_a));
                    CHECK(chan_a.contains_equivalent(chan_b));
                    CHECK_FALSE(chan_b.contains_equivalent(chan_a));
                }
            }

            SUBCASE("Different platforms")
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

                    auto url_a = CondaURL::parse(raw_url);
                    auto url_b = url_a;
                    auto chan_a = Channel(url_a, "somename", { "noarch", "linux-64" });
                    auto chan_b = Channel(url_b, "somename", { "osx-64" });

                    CHECK_FALSE(chan_a.contains_equivalent(chan_b));
                    CHECK_FALSE(chan_b.contains_equivalent(chan_a));
                }
            }

            SUBCASE("Packages")
            {
                using namespace conda_url_literals;

                const auto chan = Channel("https://repo.mamba.pm/"_cu, "conda-forge", { "linux-64" });
                CHECK(chan.contains_equivalent(Channel(chan.url() / "linux-64/pkg.conda", "", {})));
                CHECK_FALSE(chan.contains_equivalent(Channel(chan.url() / "osx-64/pkg.conda", "", {}))
                );

                const auto pkg_chan = Channel(chan.url() / "linux-64/foo.tar.bz2", "", {});
                CHECK(pkg_chan.contains_equivalent(pkg_chan));
                CHECK_FALSE(pkg_chan.contains_equivalent(chan));
                CHECK_FALSE(
                    pkg_chan.contains_equivalent(Channel(chan.url() / "osx-64/pkg.conda", "", {}))
                );
            }
        }

        SUBCASE("Contains package")
        {
            using namespace conda_url_literals;
            using Match = Channel::Match;

            SUBCASE("https://repo.mamba.pm/")
            {
                auto chan = Channel("https://repo.mamba.pm/"_cu, "conda-forge", { "linux-64" });
                CHECK_EQ(
                    chan.contains_package("https://repo.mamba.pm/linux-64/pkg.conda"_cu),
                    Match::Full
                );
                CHECK_EQ(
                    chan.contains_package("https://repo.mamba.pm/win-64/pkg.conda"_cu),
                    Match::InOtherPlatform
                );
                CHECK_EQ(
                    chan.contains_package("https://repo.mamba.pm/pkg.conda"_cu),
                    Match::InOtherPlatform
                );
            }

            SUBCASE("https://repo.mamba.pm/osx-64/foo.tar.gz")
            {
                auto chan = Channel("https://repo.mamba.pm/osx-64/foo.tar.bz2"_cu, "", {});
                CHECK_EQ(chan.contains_package(chan.url()), Match::Full);
                CHECK_EQ(chan.contains_package("https://repo.mamba.pm/win-64/pkg.conda"_cu), Match::No);
                CHECK_EQ(chan.contains_package("https://repo.mamba.pm/pkg.conda"_cu), Match::No);
            }

            SUBCASE("https://user:pass@repo.mamba.pm/conda-forge/")
            {
                auto chan = Channel(
                    "https://user:pass@repo.mamba.pm/conda-forge/"_cu,
                    "conda-forge",
                    { "win-64" }
                );
                CHECK_EQ(chan.contains_package(chan.url() / "win-64/pkg.conda"), Match::Full);
                CHECK_EQ(
                    chan.contains_package("https://repo.mamba.pm/conda-forge/win-64/pkg.conda"_cu),
                    Match::Full
                );
                CHECK_EQ(
                    chan.contains_package("https://repo.mamba.pm/conda-forge/osx-64/pkg.conda"_cu),
                    Match::InOtherPlatform
                );
            }
        }
    }

    TEST_CASE("Channel::resolve")
    {
        auto make_typical_params = []() -> ChannelResolveParams
        {
            auto make_channel = [](std::string_view loc, ChannelResolveParams const& params)
            { return Channel::resolve(UnresolvedChannel::parse(loc), params).at(0); };

            auto params = ChannelResolveParams{};
            params.platforms = { "linux-64", "noarch" };
            params.home_dir = "/home";
            params.current_working_dir = "/cwd";
            params.channel_alias = CondaURL::parse("https://conda.anaconda.org/");
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

        SUBCASE("/path/to/libmamba-1.4.2-hcea66bb_0.conda")
        {
            const auto path = "/path/to/libmamba-1.4.2-hcea66bb_0.conda"sv;
            auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::PackagePath);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                const auto url = "file:///path/to/libmamba-1.4.2-hcea66bb_0.conda"sv;
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), platform_list());  // Empty because package
                CHECK_EQ(chan.display_name(), url);
            }
        }

        SUBCASE("~/conda-bld/win-64/libmamba-1.4.2-hcea66bb_0.conda")
        {
            const auto path = "~/conda-bld/win-64/libmamba-1.4.2-hcea66bb_0.conda"sv;
            auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::PackagePath);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                const auto url = "file:///home/conda-bld/win-64/libmamba-1.4.2-hcea66bb_0.conda"sv;
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), platform_list());  // Empty because package
                CHECK_EQ(chan.display_name(), url);
            }

            SUBCASE("Matching channel alias")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ CondaURL::parse("file:///home/conda-bld"),
                    /* .custom_channels= */ {},
                    /* .custom_multichannels= */ {},
                    /* .authentication_db= */ {},
                    /* .home_dir= */ "/home",
                };
                CHECK_EQ(
                    Channel::resolve(uc, params).at(0).display_name(),
                    "win-64/libmamba-1.4.2-hcea66bb_0.conda"
                );
            }

            SUBCASE("Custom channel")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ CondaURL::parse("file:///home/conda-bld"),
                    /* .custom_channels= */ {},
                    /* .custom_multichannels= */ {},
                    /* .authentication_db= */ {},
                    /* .home_dir= */ "/home",
                };
                params.custom_channels.emplace(
                    "mychan",
                    Channel::resolve(UnresolvedChannel::parse("file:///home/conda-bld/"), params).at(0)
                );
                CHECK_EQ(Channel::resolve(uc, params).at(0).display_name(), "mychan");
            }
        }

        SUBCASE("./path/to/libmamba-1.4.2-hcea66bb_0.conda")
        {
            const auto path = "./path/to/libmamba-1.4.2-hcea66bb_0.conda"sv;
            auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::PackagePath);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                const auto url = "file:///cwd/path/to/libmamba-1.4.2-hcea66bb_0.conda"sv;
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), platform_list());  // Empty because package
                CHECK_EQ(chan.display_name(), url);
            }
        }

        SUBCASE("/some/folder")
        {
            const auto path = "/some/folder"sv;
            auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::Path);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                const auto url = "file:///some/folder"sv;
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), params.platforms);
                CHECK_EQ(chan.display_name(), url);
            }

            SUBCASE("With platform filers")
            {
                auto other_specs = UnresolvedChannel(
                    std::string(path),
                    { "foo-56" },
                    UnresolvedChannel::Type::Path
                );
                CHECK_EQ(
                    Channel::resolve(other_specs, ChannelResolveParams{}).at(0).platforms(),
                    other_specs.platform_filters()
                );
            }
        }

        SUBCASE("~/folder")
        {
            const auto path = "~/folder"sv;
            auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::Path);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                const auto url = "file:///home/folder"sv;
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), params.platforms);
                CHECK_EQ(chan.display_name(), url);
            }
        }

        SUBCASE("./other/folder")
        {
            const auto path = "./other/folder"sv;
            auto uc = UnresolvedChannel(std::string(path), {}, UnresolvedChannel::Type::Path);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                const auto url = "file:///cwd/other/folder"sv;
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), params.platforms);
                CHECK_EQ(chan.display_name(), url);
            }
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/linux-64/libmamba-1.4.2-hcea66bb_0.conda")
        {
            const auto url = "https://repo.mamba.pm/conda-forge/linux-64/libmamba-1.4.2-hcea66bb_0.conda"sv;
            auto uc = UnresolvedChannel(std::string(url), {}, UnresolvedChannel::Type::PackageURL);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), platform_list());  // Empty because package
                CHECK_EQ(chan.display_name(), url);
            }
        }

        SUBCASE("https://repo.mamba.pm")
        {
            const auto url = "https://repo.mamba.pm"sv;
            auto uc = UnresolvedChannel(
                std::string(url),
                { "linux-64", "noarch" },
                UnresolvedChannel::Type::URL
            );

            SUBCASE("Empty params")
            {
                auto channels = Channel::resolve(uc, ChannelResolveParams{});
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), uc.platform_filters());
                CHECK_EQ(chan.display_name(), url);
            }

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), uc.platform_filters());
                CHECK_EQ(chan.display_name(), url);
            }
        }

        SUBCASE("https://repo.mamba.pm/conda-forge")
        {
            const auto url = "https://repo.mamba.pm/conda-forge"sv;
            auto uc = UnresolvedChannel(
                std::string(url),
                { "linux-64", "noarch" },
                UnresolvedChannel::Type::URL
            );

            SUBCASE("Empty params")
            {
                auto channels = Channel::resolve(uc, ChannelResolveParams{});
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), uc.platform_filters());
                CHECK_EQ(chan.display_name(), url);
            }

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), uc.platform_filters());
                CHECK_EQ(chan.display_name(), url);
            }

            SUBCASE("Default platforms")
            {
                auto params = ChannelResolveParams{ /* .platform= */ { "rainbow-37", "noarch" } };
                CHECK_EQ(Channel::resolve(uc, params).at(0).platforms(), uc.platform_filters());

                uc.clear_platform_filters();
                CHECK_EQ(Channel::resolve(uc, params).at(0).platforms(), params.platforms);
            }

            SUBCASE("Matching channel alias")
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
                        /* .channel_alias= */ CondaURL::parse(alias),
                    };
                    CHECK_EQ(Channel::resolve(uc, params).at(0).display_name(), "conda-forge");
                }
            }

            SUBCASE("Not matching channel alias")
            {
                for (auto alias : {
                         "repo.anaconda.com"sv,
                         "ftp://repo.mamba.pm"sv,
                     })
                {
                    auto params = ChannelResolveParams{
                        /* .platform= */ {},
                        /* .channel_alias= */ CondaURL::parse(alias),
                    };
                    CHECK_EQ(Channel::resolve(uc, params).at(0).display_name(), url);
                }
            }

            SUBCASE("Custom channel")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ CondaURL::parse("https://repo.mamba.pm/"),
                    /* .custom_channels= */
                    { { "mychan", Channel::resolve(uc, ChannelResolveParams{}).at(0) } }
                };
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.display_name(), "mychan");
            }

            SUBCASE("Authentication info")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ {},
                    /* .custom_channels= */ {},
                    /* .custom_multichannels= */ {},
                    /* .authentication_db= */ { { "repo.mamba.pm", CondaToken{ "mytoken" } } },
                };

                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse("https://repo.mamba.pm/t/mytoken/conda-forge"));
                CHECK_EQ(chan.display_name(), "https://repo.mamba.pm/conda-forge");
            }

            SUBCASE("Authentication info multiple tokens")
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

                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse("https://repo.mamba.pm/t/forge-token/conda-forge"));
                CHECK_EQ(chan.display_name(), "https://repo.mamba.pm/conda-forge");
            }
        }

        SUBCASE("https://user:pass@repo.mamba.pm/conda-forge")
        {
            const auto url = "https://user:pass@repo.mamba.pm/conda-forge"sv;
            auto uc = UnresolvedChannel(std::string(url), {}, UnresolvedChannel::Type::URL);

            SUBCASE("Authentication info token")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ {},
                    /* .custom_channels= */ {},
                    /* .custom_multichannels= */ {},
                    /* .authentication_db= */ { { "repo.mamba.pm", CondaToken{ "mytoken" } } },
                };

                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(
                    chan.url(),
                    CondaURL::parse("https://user:pass@repo.mamba.pm/t/mytoken/conda-forge")
                );
                CHECK_EQ(chan.display_name(), "https://repo.mamba.pm/conda-forge");
            }

            SUBCASE("Authentication info user password")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ {},
                    /* .custom_channels= */ {},
                    /* .custom_multichannels= */ {},
                    /* .authentication_db= */
                    { { "repo.mamba.pm", BasicHTTPAuthentication{ "foo", "weak" } } },
                };

                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                // Higher precedence
                CHECK_EQ(chan.url(), CondaURL::parse("https://user:pass@repo.mamba.pm/conda-forge"));
                CHECK_EQ(chan.display_name(), "https://repo.mamba.pm/conda-forge");
            }
        }

        SUBCASE("https://repo.anaconda.com/pkgs/main")
        {
            const auto url = "https://repo.anaconda.com/pkgs/main"sv;
            auto uc = UnresolvedChannel(std::string(url), {}, UnresolvedChannel::Type::URL);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), params.platforms);
                CHECK_EQ(chan.display_name(), "pkgs/main");
            }

            SUBCASE("Matching channel alias")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ CondaURL::parse("https://repo.anaconda.com"),
                };

                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.display_name(), "pkgs/main");
            }
        }

        SUBCASE("conda-forge")
        {
            const auto name = "conda-forge"sv;
            auto uc = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(
                    chan.url(),
                    CondaURL::parse(util::path_concat(params.channel_alias.str(), name))
                );
                CHECK_EQ(chan.platforms(), params.platforms);
                CHECK_EQ(chan.display_name(), name);
            }

            SUBCASE("Authentication info user password")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ CondaURL::parse("mydomain.com/private"),
                    /* .custom_channels= */ {},
                    /* .custom_multichannels= */ {},
                    /* .authentication_db= */
                    { { "mydomain.com", BasicHTTPAuthentication{ "user", "pass" } } },
                };

                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(
                    chan.url(),
                    CondaURL::parse("https://user:pass@mydomain.com/private/conda-forge")
                );
                CHECK_EQ(chan.display_name(), name);
                CHECK_EQ(chan.platforms(), params.platforms);
            }

            SUBCASE("Custom channel")
            {
                auto params = make_typical_params();
                params.custom_channels.emplace(
                    "conda-forge",
                    Channel::resolve(UnresolvedChannel::parse("ftp://mydomain.net/conda"), params).at(0)
                );

                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                // Higher precedence.
                CHECK_EQ(chan.url(), CondaURL::parse("ftp://mydomain.net/conda"));
                CHECK_EQ(chan.display_name(), name);
                CHECK_EQ(chan.platforms(), params.platforms);
            }
        }

        SUBCASE("pkgs/main")
        {
            const auto name = "pkgs/main"sv;
            auto uc = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse("https://repo.anaconda.com/pkgs/main"));
                CHECK_EQ(chan.platforms(), params.platforms);
                CHECK_EQ(chan.display_name(), name);
            }
        }

        SUBCASE("pkgs/main/label/dev")
        {
            const auto name = "pkgs/main/label/dev"sv;
            auto specs = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse("https://repo.anaconda.com/pkgs/main/label/dev"));
                CHECK_EQ(chan.platforms(), params.platforms);
                CHECK_EQ(chan.display_name(), name);
            }
        }

        SUBCASE("testchannel/mylabel/xyz")
        {
            const auto name = "testchannel/mylabel/xyz"sv;
            auto uc = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(
                    chan.url(),
                    CondaURL::parse(util::path_concat(params.channel_alias.str(), name))
                );
                CHECK_EQ(chan.platforms(), params.platforms);
                CHECK_EQ(chan.display_name(), name);
            }

            SUBCASE("Custom channel")
            {
                auto params = make_typical_params();
                params.custom_channels.emplace(
                    "testchannel",
                    Channel::resolve(
                        UnresolvedChannel::parse("https://server.com/private/testchannel"),
                        params
                    )
                        .at(0)
                );

                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(
                    chan.url(),
                    CondaURL::parse("https://server.com/private/testchannel/mylabel/xyz")
                );
                CHECK_EQ(chan.display_name(), name);
                CHECK_EQ(chan.platforms(), params.platforms);
            }
        }

        SUBCASE("prefix-and-more")
        {
            const auto name = "prefix-and-more"sv;
            auto uc = UnresolvedChannel(std::string(name), {}, UnresolvedChannel::Type::Name);

            auto params = ChannelResolveParams{
                /* .platform= */ {},
                /* .channel_alias= */ CondaURL::parse("https://ali.as/"),
            };
            params.custom_channels.emplace(
                "prefix",
                Channel::resolve(UnresolvedChannel::parse("https://server.com/prefix"), params).at(0)
            );

            auto channels = Channel::resolve(uc, params);
            REQUIRE_EQ(channels.size(), 1);
            const auto& chan = channels.front();
            CHECK_EQ(chan.url(), CondaURL::parse("https://ali.as/prefix-and-more"));
            CHECK_EQ(chan.display_name(), name);
            CHECK_EQ(chan.platforms(), params.platforms);
        }

        SUBCASE("defaults")
        {
            const auto name = "defaults"sv;
            auto uc = UnresolvedChannel(std::string(name), { "linux-64" }, UnresolvedChannel::Type::Name);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 3);

                auto found_names = util::flat_set<std::string>();
                for (const auto& chan : channels)
                {
                    CHECK_EQ(chan.platforms(), uc.platform_filters());  // Overriden
                    found_names.insert(chan.display_name());
                }
                CHECK_EQ(found_names, util::flat_set<std::string>{ "pkgs/main", "pkgs/pro", "pkgs/r" });
            }
        }

        SUBCASE("<unknown>")
        {
            auto uc = UnresolvedChannel({}, { "linux-64" }, UnresolvedChannel::Type::Unknown);
            auto channels = Channel::resolve(uc, ChannelResolveParams{});
            REQUIRE_EQ(channels.size(), 1);
            const auto& chan = channels.front();
            CHECK_EQ(chan.url(), CondaURL());
            CHECK_EQ(chan.platforms(), platform_list());
            CHECK_EQ(chan.display_name(), "<unknown>");
        }

        SUBCASE("https://conda.anaconda.org/conda-forge/linux-64/x264-1%21164.3095-h166bdaf_2.tar.bz2")
        {
            // Version 1!164.3095 is URL encoded
            const auto url = "https://conda.anaconda.org/conda-forge/linux-64/x264-1%21164.3095-h166bdaf_2.tar.bz2"sv;
            auto uc = UnresolvedChannel(std::string(url), {}, UnresolvedChannel::Type::PackageURL);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(uc, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), platform_list());  // Empty because package
                CHECK_EQ(chan.display_name(), "conda-forge/linux-64/x264-1!164.3095-h166bdaf_2.tar.bz2");
            }
        }
    }
}
