// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_MATCH_SPEC
#define MAMBA_CORE_MATCH_SPEC

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

#include "mamba/specs/channel_spec.hpp"

namespace mamba
{
    class MatchSpec
    {
    public:

        [[nodiscard]] static auto parse_version_and_build(std::string_view s)
            -> std::tuple<std::string, std::string>;

        [[nodiscard]] static auto parse(std::string_view spec) -> MatchSpec;

        [[nodiscard]] static auto parse_url(std::string_view spec) -> MatchSpec;

        [[nodiscard]] auto channel() const -> const std::optional<specs::ChannelSpec>&;
        void set_channel(std::optional<specs::ChannelSpec> chan);

        [[nodiscard]] auto name_space() const -> const std::string&;

        [[nodiscard]] auto name() const -> const std::string&;

        [[nodiscard]] auto version() const -> const std::string&;
        void set_version(std::string ver);

        [[nodiscard]] auto build_number() const -> const std::string&;

        [[nodiscard]] auto build_string() const -> const std::string&;
        void set_build_string(std::string bs);

        [[nodiscard]] auto filename() const -> const std::string&;

        [[nodiscard]] auto url() const -> const std::string&;

        [[nodiscard]] auto optional() const -> bool;

        [[nodiscard]] auto conda_build_form() const -> std::string;
        [[nodiscard]] auto str() const -> std::string;

        [[nodiscard]] auto is_simple() const -> bool;

        [[nodiscard]] auto is_file() const -> bool;


        std::unordered_map<std::string, std::string> brackets;

        std::unordered_map<std::string, std::string> parens;

    private:

        std::optional<specs::ChannelSpec> m_channel;
        std::string m_name_space;
        std::string m_name;
        std::string m_version;
        std::string m_build_number;
        std::string m_build_string;
        // TODO can put inside channel spec
        std::string m_filename;
        std::string m_url;
        bool m_optional = false;
    };
}
#endif
