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

        [[nodiscard]] auto conda_build_form() const -> std::string;
        [[nodiscard]] auto str() const -> std::string;

        [[nodiscard]] auto is_simple() const -> bool;

        std::optional<specs::ChannelSpec> channel;
        std::string name;
        std::string version;
        std::string ns;
        std::string build_string;
        std::string fn;
        std::string url;
        std::string build_number;

        bool is_file = false;
        bool optional = false;
        std::unordered_map<std::string, std::string> brackets;
        std::unordered_map<std::string, std::string> parens;
    };
}
#endif
