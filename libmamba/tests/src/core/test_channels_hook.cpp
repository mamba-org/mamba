// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <fstream>
#include <vector>

#include <catch2/catch_all.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"

using mamba::unindent;

#include "mambatests.hpp"

namespace
{
    bool
    channels_equal(const std::vector<std::string>& actual, const std::vector<std::string>& expected)
    {
        return actual == expected;
    }

    bool channels_contain_nodefaults(const std::vector<std::string>& channels)
    {
        return std::find(channels.begin(), channels.end(), "nodefaults") != channels.end();
    }
}

namespace mamba::testing
{
    class ChannelsHookFixture
    {
    public:

        ChannelsHookFixture()
        {
            m_context_change.preserve(&mamba::Context::channel_alias).preserve(&mamba::Context::channels);
        }

        ~ChannelsHookFixture()
        {
            config.reset_configurables();
            mamba::util::unset_env("CONDA_CHANNELS");
        }

        void load_rc(const std::string& rc)
        {
            const auto unique_location = tempfile_ptr->path();
            std::ofstream out_file(unique_location.std_path(), std::ofstream::out | std::ofstream::trunc);
            out_file << rc;
            out_file.close();

            config.reset_configurables();
            config.at("rc_files").set_value<std::vector<fs::u8path>>({ unique_location });
            config.load();
        }

        void
        load_rc_and_cli_channels(const std::string& rc, const std::vector<std::string>& cli_channels)
        {
            const auto unique_location = tempfile_ptr->path();
            std::ofstream out_file(unique_location.std_path(), std::ofstream::out | std::ofstream::trunc);
            out_file << rc;
            out_file.close();

            config.reset_configurables();
            config.at("rc_files").set_value<std::vector<fs::u8path>>({ unique_location });
            config.at("channels").set_cli_value(cli_channels);
            config.load();
        }

        void load_env_file(const std::string& rc, const std::string& env_yaml)
        {
            const auto rc_location = tempfile_ptr->path();
            std::ofstream rc_file(rc_location.std_path(), std::ofstream::out | std::ofstream::trunc);
            rc_file << rc;
            rc_file.close();

            const auto env_location = envfile_ptr->path();
            std::ofstream env_file(env_location.std_path(), std::ofstream::out | std::ofstream::trunc);
            env_file << env_yaml;
            env_file.close();

            config.reset_configurables();
            config.at("rc_files").set_value<std::vector<fs::u8path>>({ rc_location });
            config.at("file_specs").set_value(std::vector<std::string>{ env_location.string() });
            config.load();
        }

        const std::vector<std::string>& resolved_channels() const
        {
            return ctx.channels;
        }

        mamba::Context& ctx = mambatests::context();
        mamba::Configuration config{ ctx };
        mambatests::ScopedContextChange m_context_change{ ctx };

    private:

        std::unique_ptr<TemporaryFile> tempfile_ptr = std::make_unique<TemporaryFile>("mambarc", ".yaml");
        std::unique_ptr<TemporaryFile> envfile_ptr = std::make_unique<TemporaryFile>("env", ".yaml");
    };
}  // namespace mamba::testing

TEST_CASE("channels_hook strips nodefaults from rc configuration", "[mamba::api][channels_hook]")
{
    using mamba::testing::ChannelsHookFixture;

    SECTION("conda-forge and nodefaults in rc (user regression)")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc(unindent(R"(
            channels:
              - conda-forge
              - nodefaults
            channel_priority: strict
        )"));

        REQUIRE_FALSE(channels_contain_nodefaults(fixture.resolved_channels()));
        REQUIRE(channels_equal(fixture.resolved_channels(), { "conda-forge" }));
    }

    SECTION("nodefaults between other rc channels")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc(unindent(R"(
            channels:
              - conda-forge
              - nodefaults
              - bioconda
        )"));

        REQUIRE(channels_equal(fixture.resolved_channels(), { "conda-forge", "bioconda" }));
    }

    SECTION("nodefaults only in rc")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc(unindent(R"(
            channels:
              - nodefaults
        )"));

        REQUIRE(fixture.resolved_channels().empty());
    }

    SECTION("duplicate nodefaults entries in rc")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc(unindent(R"(
            channels:
              - nodefaults
              - conda-forge
              - nodefaults
              - pytorch
        )"));

        REQUIRE(channels_equal(fixture.resolved_channels(), { "conda-forge", "pytorch" }));
    }

    SECTION("rc without nodefaults is unchanged")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc(unindent(R"(
            channels:
              - conda-forge
              - bioconda
        )"));

        REQUIRE(channels_equal(fixture.resolved_channels(), { "conda-forge", "bioconda" }));
    }
}

TEST_CASE("channels_hook merges rc and cli channels when nodefaults is rc-only", "[mamba::api][channels_hook]")
{
    using mamba::testing::ChannelsHookFixture;

    SECTION("cli channels are prepended and rc nodefaults is stripped")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc_and_cli_channels(
            unindent(R"(
                channels:
                  - bioconda
                  - nodefaults
            )"),
            { "https://prefix.dev/emscripten-forge-dev", "conda-forge" }
        );

        REQUIRE_FALSE(channels_contain_nodefaults(fixture.resolved_channels()));
        REQUIRE(channels_equal(
            fixture.resolved_channels(),
            { "https://prefix.dev/emscripten-forge-dev", "conda-forge", "bioconda" }
        ));
    }

    SECTION("cli channel without nodefaults does not override rc channels")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc_and_cli_channels(
            unindent(R"(
                channels:
                  - rc-channel
                  - nodefaults
            )"),
            { "cli-channel" }
        );

        REQUIRE(channels_equal(fixture.resolved_channels(), { "cli-channel", "rc-channel" }));
    }
}

TEST_CASE("channels_hook overrides channels when nodefaults is cli-configured", "[mamba::api][channels_hook]")
{
    using mamba::testing::ChannelsHookFixture;

    SECTION("cli nodefaults drops rc channels")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc_and_cli_channels(
            unindent(R"(
                channels:
                  - rc-channel
                  - another-rc
            )"),
            { "cli-channel", "nodefaults" }
        );

        REQUIRE(channels_equal(fixture.resolved_channels(), { "cli-channel" }));
    }

    SECTION("cli nodefaults keeps other cli channels in order")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc_and_cli_channels(
            unindent(R"(
                channels:
                  - rc-channel
            )"),
            { "first", "second", "nodefaults", "third" }
        );

        REQUIRE(channels_equal(fixture.resolved_channels(), { "first", "second", "third" }));
    }

    SECTION("cli nodefaults only yields empty channel list")
    {
        ChannelsHookFixture fixture;
        fixture.load_rc_and_cli_channels(
            unindent(R"(
                channels:
                  - rc-channel
            )"),
            { "nodefaults" }
        );

        REQUIRE(fixture.resolved_channels().empty());
    }
}

TEST_CASE("channels_hook handles nodefaults from environment yaml files", "[mamba::api][channels_hook]")
{
    using mamba::testing::ChannelsHookFixture;

    SECTION("environment yaml nodefaults overrides rc channels")
    {
        ChannelsHookFixture fixture;
        fixture.load_env_file(
            unindent(R"(
                channels:
                  - rc-channel
            )"),
            unindent(R"(
                channels:
                  - yaml-channel
                  - nodefaults
                dependencies:
                  - python
            )")
        );

        REQUIRE(channels_equal(fixture.resolved_channels(), { "yaml-channel" }));
    }

    SECTION("environment yaml nodefaults with multiple yaml channels")
    {
        ChannelsHookFixture fixture;
        fixture.load_env_file(
            unindent(R"(
                channels:
                  - rc-channel
                  - nodefaults
            )"),
            unindent(R"(
                channels:
                  - yaml-a
                  - yaml-b
                  - nodefaults
                dependencies:
                  - python
            )")
        );

        REQUIRE(channels_equal(fixture.resolved_channels(), { "yaml-a", "yaml-b" }));
    }
}

TEST_CASE("channels_hook strips nodefaults from CONDA_CHANNELS", "[mamba::api][channels_hook]")
{
    using mamba::testing::ChannelsHookFixture;

    ChannelsHookFixture fixture;
    mamba::util::set_env("CONDA_CHANNELS", "env-a,nodefaults,env-b");
    fixture.load_rc(unindent(R"(
        channels:
          - rc-channel
          - nodefaults
    )"));

    REQUIRE_FALSE(channels_contain_nodefaults(fixture.resolved_channels()));
    REQUIRE(channels_equal(fixture.resolved_channels(), { "env-a", "env-b", "rc-channel" }));
}

TEST_CASE("channels_hook direct invocation", "[mamba::api][channels_hook]")
{
    mamba::Context& ctx = mambatests::context();
    mamba::Configuration config{ ctx };

    SECTION("strip nodefaults from merged channels when cli has no nodefaults")
    {
        config.at("channels").set_cli_value(std::vector<std::string>{ "cli-a", "cli-b" });
        std::vector<std::string> channels = { "cli-a", "cli-b", "rc-a", "nodefaults", "rc-b" };
        mamba::detail::channels_hook(config, channels);
        REQUIRE(channels_equal(channels, { "cli-a", "cli-b", "rc-a", "rc-b" }));
    }

    SECTION("cli nodefaults replaces merged channels with cli-only list")
    {
        config.reset_configurables();
        config.at("channels")
            .set_cli_value(std::vector<std::string>{ "override-a", "nodefaults", "override-b" });
        std::vector<std::string> channels = { "rc-a", "rc-b", "nodefaults" };
        mamba::detail::channels_hook(config, channels);
        REQUIRE(channels_equal(channels, { "override-a", "override-b" }));
    }

    SECTION("rc-only nodefaults is stripped from passed channels vector")
    {
        config.reset_configurables();
        std::vector<std::string> channels = { "conda-forge", "nodefaults" };
        mamba::detail::channels_hook(config, channels);
        REQUIRE(channels_equal(channels, { "conda-forge" }));
    }

    SECTION("multiple nodefaults entries are all removed")
    {
        config.reset_configurables();
        std::vector<std::string> channels = { "nodefaults", "a", "nodefaults", "b", "nodefaults" };
        mamba::detail::channels_hook(config, channels);
        REQUIRE(channels_equal(channels, { "a", "b" }));
    }

    SECTION("empty channel list stays empty")
    {
        config.reset_configurables();
        std::vector<std::string> channels;
        mamba::detail::channels_hook(config, channels);
        REQUIRE(channels.empty());
    }

    SECTION("channels without nodefaults are unchanged")
    {
        config.reset_configurables();
        std::vector<std::string> channels = { "conda-forge", "bioconda", "pytorch" };
        mamba::detail::channels_hook(config, channels);
        REQUIRE(channels_equal(channels, { "conda-forge", "bioconda", "pytorch" }));
    }

    SECTION("nodefaults-looking channel names are not stripped")
    {
        config.reset_configurables();
        std::vector<std::string> channels = { "nodefaults-extra", "my-nodefaults" };
        mamba::detail::channels_hook(config, channels);
        REQUIRE(channels_equal(channels, { "nodefaults-extra", "my-nodefaults" }));
    }
}
