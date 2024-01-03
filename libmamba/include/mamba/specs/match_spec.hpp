// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_MATCH_SPEC
#define MAMBA_SPECS_MATCH_SPEC

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

#include "mamba/specs/build_number_spec.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/glob_spec.hpp"
#include "mamba/specs/version_spec.hpp"

namespace mamba::specs
{
    class MatchSpec
    {
    public:

        using NameSpec = GlobSpec;
        using BuildStringSpec = GlobSpec;

        [[nodiscard]] static auto parse(std::string_view spec) -> MatchSpec;

        [[nodiscard]] static auto parse_url(std::string_view spec) -> MatchSpec;

        [[nodiscard]] auto channel() const -> const std::optional<ChannelSpec>&;
        void set_channel(std::optional<ChannelSpec> chan);

        [[nodiscard]] auto name_space() const -> const std::string&;
        void set_name_space(std::string ns);

        [[nodiscard]] auto name() const -> const NameSpec&;
        void set_name(NameSpec name);

        [[nodiscard]] auto version() const -> const VersionSpec&;
        void set_version(VersionSpec ver);

        [[nodiscard]] auto build_number() const -> const BuildNumberSpec&;
        void set_build_number(BuildNumberSpec num);

        [[nodiscard]] auto build_string() const -> const BuildStringSpec&;
        void set_build_string(BuildStringSpec bs);

        [[nodiscard]] auto optional() const -> bool;
        void set_optional(bool opt);

        [[nodiscard]] auto filename() const -> const std::string&;

        [[nodiscard]] auto url() const -> const std::string&;

        [[nodiscard]] auto conda_build_form() const -> std::string;
        [[nodiscard]] auto str() const -> std::string;

        [[nodiscard]] auto is_simple() const -> bool;

        [[nodiscard]] auto is_file() const -> bool;

        std::unordered_map<std::string, std::string> brackets;

        std::unordered_map<std::string, std::string> parens;

    private:

        std::optional<ChannelSpec> m_channel;
        VersionSpec m_version;
        NameSpec m_name;
        BuildStringSpec m_build_string;
        std::string m_name_space;
        BuildNumberSpec m_build_number;
        // TODO can put inside channel spec
        std::string m_filename;
        std::string m_url;
        bool m_optional = false;
    };
}
#endif
