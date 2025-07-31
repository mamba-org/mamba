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
#include <fmt/format.h>

#include "mamba/specs/build_number_spec.hpp"
#include "mamba/specs/chimera_string_spec.hpp"
#include "mamba/specs/error.hpp"
#include "mamba/specs/glob_spec.hpp"
#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/specs/version_spec.hpp"
#include "mamba/util/flat_set.hpp"
#include "mamba/util/heap_optional.hpp"

namespace mamba::specs
{
    class PackageInfo;

    class MatchSpec
    {
    public:

        using NameSpec = GlobSpec;
        using BuildStringSpec = ChimeraStringSpec;
        using platform_set = typename UnresolvedChannel::platform_set;
        using platform_set_const_ref = std::reference_wrapper<const platform_set>;
        using string_set = typename util::flat_set<std::string>;
        using string_set_const_ref = typename std::reference_wrapper<const string_set>;

        inline static constexpr char url_md5_sep = '#';
        inline static constexpr char preferred_list_open = '[';
        inline static constexpr char preferred_list_close = ']';
        inline static constexpr char alt_list_open = '(';
        inline static constexpr char alt_list_close = ')';
        inline static constexpr char preferred_quote = '"';
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
        [[nodiscard]] auto to_string() const -> std::string;

        /**
         * Return true if the MatchSpec can be written as ``<name> <version> <build_string>``.
         */
        [[nodiscard]] auto is_simple() const -> bool;

        /**
         * Return true if the MatchSpec contains an exact package name and nothing else.
         */
        [[nodiscard]] auto is_only_package_name() const -> bool;

        /**
         * Make a new MatchSpec that matches only on the name part.
         */
        [[nodiscard]] auto to_named_spec() const -> MatchSpec;

        /**
         * Check if the MatchSpec matches the given package.
         *
         * The check exclude anything related to the channel, du to the difficulties in
         * comparing unresolved channels and the fact that this check can be also be done once
         * at a repository level when the user knows how packages are organised.
         *
         * This function is written as a generic template, to accommodate various uses: the fact
         * that the attributes may not always be in the correct format in the package, and that
         * their parsing may be cached.
         */
        template <typename Pkg>
        [[nodiscard]] auto contains_except_channel(const Pkg& pkg) const -> bool;

        /**
         * Convenience wrapper making necessary conversions.
         */
        [[nodiscard]] auto contains_except_channel(const PackageInfo& pkg) const -> bool;

        /**
         * Naive attribute-wise comparison.
         *
         * @warning Some complex matchspec could compare to false but actually represent the same
         * set of packages. This strong equality is hard to detect.
         */
        [[nodiscard]] auto operator==(const MatchSpec& other) const -> bool = default;

        [[nodiscard]] auto operator!=(const MatchSpec& other) const -> bool = default;

        auto extra_members_hash() const -> std::size_t;

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

            // TODO(C++20): replace by the `= default` implementation of `operator==`
            [[nodiscard]] auto operator==(const ExtraMembers& other) const -> bool
            {
                return filename == other.filename                 //
                       && subdirs == other.subdirs                //
                       && md5 == other.md5                        //
                       && sha256 == other.sha256                  //
                       && license == other.license                //
                       && license_family == other.license_family  //
                       && features == other.features              //
                       && track_features == other.track_features  //
                       && optional == other.optional;
            }

            [[nodiscard]] auto operator!=(const ExtraMembers& other) const -> bool
            {
                return !(*this == other);
            }

            friend struct std::hash<ExtraMembers>;
        };
        friend struct std::hash<MatchSpec::ExtraMembers>;

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
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        // make sure that range is empty
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
        {
            throw fmt::format_error("Invalid format");
        }
        return ctx.begin();
    }

    auto format(const ::mamba::specs::MatchSpec& spec, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct std::hash<mamba::specs::MatchSpec>
{
    auto operator()(const mamba::specs::MatchSpec& spec) const -> std::size_t;
};

template <>
struct std::hash<mamba::specs::MatchSpec::ExtraMembers>
{
    auto operator()(const mamba::specs::MatchSpec::ExtraMembers& extra) const -> std::size_t;
};

/*********************************
 *  Implementation of MatchSpec  *
 *********************************/

namespace mamba::specs
{
    namespace detail
    {
        template <typename Return>
        struct Deref
        {
            template <typename T>
            static auto deref(T&& x) -> decltype(auto)
            {
                return x;
            }
        };

        template <typename Inner>
        struct Deref<std::reference_wrapper<Inner>>
        {
            template <typename T>
            static auto deref(T&& x) -> decltype(auto)
            {
                return std::forward<T>(x).get();
            }
        };

        template <typename Attr, typename Pkg>
        auto invoke_pkg(Attr&& attr, Pkg&& pkg) -> decltype(auto)
        {
            using Return = std::decay_t<std::invoke_result_t<Attr&&, Pkg&&>>;
            return Deref<Return>::deref(std::invoke(std::forward<Attr>(attr), std::forward<Pkg>(pkg)));
        }
    }

    template <typename Pkg>
    auto MatchSpec::contains_except_channel(const Pkg& pkg) const -> bool
    {
        if (                                                                                  //
            !name().contains(detail::invoke_pkg(&Pkg::name, pkg))                             //
            || !version().contains(detail::invoke_pkg(&Pkg::version, pkg))                    //
            || !build_string().contains(detail::invoke_pkg(&Pkg::build_string, pkg))          //
            || !build_number().contains(detail::invoke_pkg(&Pkg::build_number, pkg))          //
            || (!md5().empty() && (md5() != detail::invoke_pkg(&Pkg::md5, pkg)))              //
            || (!sha256().empty() && (sha256() != detail::invoke_pkg(&Pkg::sha256, pkg)))     //
            || (!license().empty() && (license() != detail::invoke_pkg(&Pkg::license, pkg)))  //
        )
        {
            return false;
        }

        if (const auto& plats = platforms();
            plats.has_value() && !plats->get().contains(detail::invoke_pkg(&Pkg::platform, pkg)))
        {
            return false;
        }

        if (const auto& tfeats = track_features();
            tfeats.has_value()
            && !util::set_is_subset_of(tfeats->get(), detail::invoke_pkg(&Pkg::track_features, pkg)))
        {
            return false;
        }

        return true;
    }
}

#endif
