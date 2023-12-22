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

        [[nodiscard]] auto name() const -> const std::string&;

        [[nodiscard]] auto name_space() const -> const std::string&;

        [[nodiscard]] auto filename() const -> const std::string&;

        [[nodiscard]] auto url() const -> const std::string&;

        [[nodiscard]] auto conda_build_form() const -> std::string;
        [[nodiscard]] auto str() const -> std::string;

        [[nodiscard]] auto is_simple() const -> bool;

        [[nodiscard]] auto is_file() const -> bool;


        std::optional<specs::ChannelSpec> channel;
        std::string version;
        std::string build_string;
        std::string build_number;

        bool optional = false;
        std::unordered_map<std::string, std::string> brackets;

        std::unordered_map<std::string, std::string> parens;

    private:

        std::string m_name;
        std::string m_name_space;
        // TODO can put inside channel spec
        std::string m_filename;
        std::string m_url;
    };
}
#endif
