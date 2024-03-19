// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_LIBSOLV_MATCHER
#define MAMBA_SOLVER_LIBSOLV_MATCHER

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "mamba/core/error_handling.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/version.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/solvable.hpp"

namespace mamba::solver::libsolv
{
    struct MatchFlags
    {
        bool skip_installed = false;

        /**
         * Deserialization for internal use mamba use, should not be loaded from disk.
         */
        static auto internal_deserialize(std::string_view) -> MatchFlags;

        /**
         * Serialization for internal use mamba use, should not be saved to disk.
         */
        void internal_serialize_to(std::string& out) const;
        [[nodiscard]] auto internal_serialize() const -> std::string;
    };

    class Matcher
    {
    public:

        explicit Matcher(specs::ChannelResolveParams channel_params);

        [[nodiscard]] auto channel_params() const -> const specs::ChannelResolveParams&;

        auto get_matching_packages(  //
            solv::ObjPoolView pool,
            const specs::MatchSpec& ms,
            const MatchFlags& flags = {}
        ) -> solv::OffsetId;

        auto get_matching_packages(  //
            solv::ObjPoolView pool,
            std::string_view dep,
            const MatchFlags& flags = {}
        ) -> solv::OffsetId;

    private:

        using channel_list = specs::ChannelResolveParams::channel_list;
        using channel_list_const_ref = std::reference_wrapper<const channel_list>;

        struct Pkg
        {
            std::string_view name;
            std::reference_wrapper<const specs::Version> version;
            std::string_view build_string;
            std::size_t build_number;
            std::string_view md5;
            std::string_view sha256;
            std::string_view license;
            std::string platform;
            specs::MatchSpec::string_set track_features;
        };

        auto get_pkg_attributes(  //
            solv::ObjPoolView pool,
            solv::ObjSolvableViewConst solv
        ) -> expected_t<Pkg>;

        auto pkg_match_except_channel(  //
            solv::ObjPoolView pool,
            solv::ObjSolvableViewConst solv,
            const specs::MatchSpec& ms
        ) -> bool;

        auto get_channels(const specs::UnresolvedChannel& uc) -> expected_t<channel_list_const_ref>;
        auto get_channels(std::string_view chan) -> expected_t<channel_list_const_ref>;

        auto pkg_match_channels(  //
            solv::ObjSolvableViewConst solv,
            const channel_list& channels
        ) -> bool;

        auto pkg_match_channels(  //
            solv::ObjSolvableViewConst solv,
            const specs::MatchSpec& ms
        ) -> bool;

        specs::ChannelResolveParams m_channel_params;
        solv::ObjQueue m_packages_buffer = {};
        // No need for matchspec cache since they have the same string id they should be handled
        // by libsolv.
        std::unordered_map<std::string, specs::Version> m_version_cache = {};
        std::unordered_map<std::string, channel_list> m_channel_cache = {};
    };
}
#endif
