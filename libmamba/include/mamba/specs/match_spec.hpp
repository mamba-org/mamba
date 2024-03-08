// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_MATCH_SPEC
#define MAMBA_SPECS_MATCH_SPEC

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <fmt/core.h>

#include "mamba/specs/build_number_spec.hpp"
#include "mamba/specs/error.hpp"
#include "mamba/specs/glob_spec.hpp"
#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/specs/version_spec.hpp"
#include "mamba/util/flat_set.hpp"
#include "mamba/util/heap_optional.hpp"

namespace mamba::specs
{
    class MatchSpec
    {
    public:

        using NameSpec = GlobSpec;
        using BuildStringSpec = GlobSpec;
        using platform_set = typename UnresolvedChannel::platform_set;
        using platform_set_const_ref = std::reference_wrapper<const platform_set>;
        using string_set = typename util::flat_set<std::string>;
        using string_set_const_ref = typename std::reference_wrapper<const string_set>;

        inline static constexpr char url_md5_sep = '#';
        inline static constexpr char prefered_list_open = '[';
        inline static constexpr char prefered_list_close = ']';
        inline static constexpr char alt_list_open = '(';
        inline static constexpr char alt_list_close = ')';
        inline static constexpr char prefered_quote = '"';
        inline static constexpr char alt_quote = '\'';
        inline static constexpr char channel_namespace_spec_sep = ':';
        inline static constexpr char attribute_sep = ',';
        inline static constexpr char attribute_assign = '=';
        inline static constexpr auto package_version_sep = std::array{ ' ', '=', '<', '>', '~', '!' };
        inline static constexpr auto feature_sep = std::array{ ' ', ',' };


        [[nodiscard]] static auto parse(std::string_view spec) -> expected_parse_t<MatchSpec>;

        [[nodiscard]] static auto parse_url(std::string_view spec) -> expected_parse_t<MatchSpec>;

        [[nodiscard]] auto channel() const -> const std::optional<UnresolvedChannel>&;
        void set_channel(std::optional<UnresolvedChannel> chan);

        [[nodiscard]] auto filename() const -> std::string_view;
        void set_filename(std::string val);

        [[nodiscard]] auto is_file() const -> bool;

        [[nodiscard]] auto platforms() const -> std::optional<platform_set_const_ref>;
        void set_platforms(platform_set val);

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

        [[nodiscard]] auto md5() const -> std::string_view;
        void set_md5(std::string val);

        [[nodiscard]] auto sha256() const -> std::string_view;
        void set_sha256(std::string val);

        [[nodiscard]] auto license() const -> std::string_view;
        void set_license(std::string val);

        [[nodiscard]] auto license_family() const -> std::string_view;
        void set_license_family(std::string val);

        [[nodiscard]] auto features() const -> std::string_view;
        void set_features(std::string val);

        [[nodiscard]] auto track_features() const -> std::optional<string_set_const_ref>;
        void set_track_features(string_set val);

        [[nodiscard]] auto optional() const -> bool;
        void set_optional(bool opt);

        [[nodiscard]] auto conda_build_form() const -> std::string;
        [[nodiscard]] auto str() const -> std::string;

        [[nodiscard]] auto is_simple() const -> bool;

    private:

        struct ExtraMembers
        {
            // The filename is stored as part of the channel when it is a full Package URL
            std::string filename = {};
            // The filename is stored as part of the channel when it is available
            platform_set subdirs = {};
            std::string md5 = {};
            std::string sha256 = {};
            std::string license = {};
            std::string license_family = {};
            std::string features = {};
            string_set track_features = {};
            bool optional = false;
        };

        std::optional<UnresolvedChannel> m_channel;
        VersionSpec m_version;
        NameSpec m_name;
        BuildStringSpec m_build_string;
        std::string m_name_space;
        BuildNumberSpec m_build_number;
        util::heap_optional<ExtraMembers> m_extra = {};  // unlikely data

        auto extra() -> ExtraMembers&;

        [[nodiscard]] auto channel_is_file() const -> bool;
        [[nodiscard]] auto channel_filename() const -> std::string_view;
        void set_channel_filename(std::string val);

        [[nodiscard]] auto extra_filename() const -> std::string_view;
        void set_extra_filename(std::string val);

        [[nodiscard]] auto extra_subdirs() const -> std::optional<platform_set_const_ref>;
        void set_extra_subdirs(platform_set val);
    };

    namespace match_spec_literals
    {
        auto operator""_ms(const char* str, std::size_t len) -> MatchSpec;
    }
}

template <>
struct fmt::formatter<::mamba::specs::MatchSpec>
{
    auto parse(format_parse_context& ctx) -> decltype(ctx.begin());

    auto format(const ::mamba::specs::MatchSpec& spec, format_context& ctx) -> decltype(ctx.out());
};
#endif
