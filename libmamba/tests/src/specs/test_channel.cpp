// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/channel.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
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

    TEST_CASE("Channel constructor")
    {
        SUBCASE("Trailing slash")
        {
            // Leading slash for empty paths
            for (auto url : {
                     "https://repo.mamba.pm/"sv,
                     "https://repo.mamba.pm"sv,
                 })
            {
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
                auto chan = Channel(CondaURL::parse(url), "somename");
                CHECK_EQ(chan.url().str(), mamba::util::rstrip(url, '/'));
            }
        }
    }

    TEST_CASE("Channel::resolve")
    {
        auto make_typical_params = []() -> ChannelResolveParams
        {
            auto make_channel = [](std::string_view loc, ChannelResolveParams const& params)
            { return Channel::resolve(ChannelSpec::parse(loc), params).at(0); };

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
            auto specs = ChannelSpec(std::string(path), {}, ChannelSpec::Type::PackagePath);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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
            auto specs = ChannelSpec(std::string(path), {}, ChannelSpec::Type::PackagePath);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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
                    Channel::resolve(specs, params).at(0).display_name(),
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
                    Channel::resolve(ChannelSpec::parse("file:///home/conda-bld/"), params).at(0)
                );
                CHECK_EQ(Channel::resolve(specs, params).at(0).display_name(), "mychan");
            }
        }

        SUBCASE("./path/to/libmamba-1.4.2-hcea66bb_0.conda")
        {
            const auto path = "./path/to/libmamba-1.4.2-hcea66bb_0.conda"sv;
            auto specs = ChannelSpec(std::string(path), {}, ChannelSpec::Type::PackagePath);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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
            auto specs = ChannelSpec(std::string(path), {}, ChannelSpec::Type::Path);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                const auto url = "file:///some/folder"sv;
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), params.platforms);
                CHECK_EQ(chan.display_name(), url);
            }

            SUBCASE("With platform filers")
            {
                auto other_specs = ChannelSpec(std::string(path), { "foo-56" }, ChannelSpec::Type::Path);
                CHECK_EQ(
                    Channel::resolve(other_specs, ChannelResolveParams{}).at(0).platforms(),
                    other_specs.platform_filters()
                );
            }
        }

        SUBCASE("~/folder")
        {
            const auto path = "~/folder"sv;
            auto specs = ChannelSpec(std::string(path), {}, ChannelSpec::Type::Path);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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
            auto specs = ChannelSpec(std::string(path), {}, ChannelSpec::Type::Path);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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
            auto specs = ChannelSpec(std::string(url), {}, ChannelSpec::Type::PackageURL);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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
            auto specs = ChannelSpec(std::string(url), { "linux-64", "noarch" }, ChannelSpec::Type::URL);

            SUBCASE("Empty params")
            {
                auto channels = Channel::resolve(specs, ChannelResolveParams{});
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), specs.platform_filters());
                CHECK_EQ(chan.display_name(), url);
            }

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), specs.platform_filters());
                CHECK_EQ(chan.display_name(), url);
            }
        }

        SUBCASE("https://repo.mamba.pm/conda-forge")
        {
            const auto url = "https://repo.mamba.pm/conda-forge"sv;
            auto specs = ChannelSpec(std::string(url), { "linux-64", "noarch" }, ChannelSpec::Type::URL);

            SUBCASE("Empty params")
            {
                auto channels = Channel::resolve(specs, ChannelResolveParams{});
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), specs.platform_filters());
                CHECK_EQ(chan.display_name(), url);
            }

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.platforms(), specs.platform_filters());
                CHECK_EQ(chan.display_name(), url);
            }

            SUBCASE("Default platforms")
            {
                auto params = ChannelResolveParams{ /* .platform= */ { "rainbow-37", "noarch" } };
                CHECK_EQ(Channel::resolve(specs, params).at(0).platforms(), specs.platform_filters());

                specs.clear_platform_filters();
                CHECK_EQ(Channel::resolve(specs, params).at(0).platforms(), params.platforms);
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
                    CHECK_EQ(Channel::resolve(specs, params).at(0).display_name(), "conda-forge");
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
                    CHECK_EQ(Channel::resolve(specs, params).at(0).display_name(), url);
                }
            }

            SUBCASE("Custom channel")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ CondaURL::parse("https://repo.mamba.pm/"),
                    /* .custom_channels= */
                    { { "mychan", Channel::resolve(specs, ChannelResolveParams{}).at(0) } }
                };
                auto channels = Channel::resolve(specs, params);
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

                auto channels = Channel::resolve(specs, params);
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

                auto channels = Channel::resolve(specs, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse("https://repo.mamba.pm/t/forge-token/conda-forge"));
                CHECK_EQ(chan.display_name(), "https://repo.mamba.pm/conda-forge");
            }
        }

        SUBCASE("https://user:pass@repo.mamba.pm/conda-forge")
        {
            const auto url = "https://user:pass@repo.mamba.pm/conda-forge"sv;
            auto specs = ChannelSpec(std::string(url), {}, ChannelSpec::Type::URL);

            SUBCASE("Authentication info token")
            {
                auto params = ChannelResolveParams{
                    /* .platform= */ {},
                    /* .channel_alias= */ {},
                    /* .custom_channels= */ {},
                    /* .custom_multichannels= */ {},
                    /* .authentication_db= */ { { "repo.mamba.pm", CondaToken{ "mytoken" } } },
                };

                auto channels = Channel::resolve(specs, params);
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

                auto channels = Channel::resolve(specs, params);
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
            auto specs = ChannelSpec(std::string(url), {}, ChannelSpec::Type::URL);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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

                auto channels = Channel::resolve(specs, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                CHECK_EQ(chan.url(), CondaURL::parse(url));
                CHECK_EQ(chan.display_name(), "pkgs/main");
            }
        }

        SUBCASE("conda-forge")
        {
            const auto name = "conda-forge"sv;
            auto specs = ChannelSpec(std::string(name), {}, ChannelSpec::Type::Name);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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

                auto channels = Channel::resolve(specs, params);
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
                    Channel::resolve(ChannelSpec::parse("ftp://mydomain.net/conda"), params).at(0)
                );

                auto channels = Channel::resolve(specs, params);
                REQUIRE_EQ(channels.size(), 1);
                const auto& chan = channels.front();
                // Higher precedence. Unfotunate, but the name must be repeated...
                CHECK_EQ(chan.url(), CondaURL::parse("ftp://mydomain.net/conda/conda-forge"));
                CHECK_EQ(chan.display_name(), name);
                CHECK_EQ(chan.platforms(), params.platforms);
            }
        }

        SUBCASE("pkgs/main")
        {
            const auto name = "pkgs/main"sv;
            auto specs = ChannelSpec(std::string(name), {}, ChannelSpec::Type::Name);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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
            auto specs = ChannelSpec(std::string(name), {}, ChannelSpec::Type::Name);

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
            auto specs = ChannelSpec(std::string(name), {}, ChannelSpec::Type::Name);

            SUBCASE("Typical parameters")
            {
                auto params = make_typical_params();
                auto channels = Channel::resolve(specs, params);
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
                    Channel::resolve(ChannelSpec::parse("https://server.com/private/testchannel"), params)
                        .at(0)
                );

                auto channels = Channel::resolve(specs, params);
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

        SUBCASE("<unknown>")
        {
            auto specs = ChannelSpec({}, { "linux-64" }, ChannelSpec::Type::Unknown);
            auto channels = Channel::resolve(specs, ChannelResolveParams{});
            REQUIRE_EQ(channels.size(), 1);
            const auto& chan = channels.front();
            CHECK_EQ(chan.url(), CondaURL());
            CHECK_EQ(chan.platforms(), platform_list());
            CHECK_EQ(chan.display_name(), "<unknown>");
        }
    }
}
