// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/format.h>

#include "solver/libsolv/matcher.hpp"

namespace mamba::solver::libsolv
{
    /**********************************
     *  Implementation of MatchFlags  *
     **********************************/

    auto MatchFlags::internal_deserialize(std::string_view in) -> MatchFlags
    {
        auto out = MatchFlags{};
        if (in.size() >= 1)
        {
            out.skip_installed = in[0] == '1';
        }
        return out;
    }

    void MatchFlags::internal_serialize_to(std::string& out) const
    {
        // We simply write a bitset for flags
        out.push_back(skip_installed ? '1' : '0');
    }

    [[nodiscard]] auto MatchFlags::internal_serialize() const -> std::string
    {
        auto out = std::string();
        internal_serialize_to(out);
        return out;
    }

    /*******************************
     *  Implementation of Matcher  *
     *******************************/

    auto Matcher::get_matching_packages(  //
        solv::ObjPoolView pool,
        const specs::MatchSpec& ms,
        const MatchFlags& flags

    ) -> solv::OffsetId
    {
        m_packages.clear();  // Reuse the buffer

        auto add_pkg_if_matching = [&](solv::ObjSolvableViewConst s)
        {
            if (flags.skip_installed && s.installed())
            {
                return;
            }

            if (pkg_match(pool, s, ms))
            {
                m_packages.push_back(s.id());
            }
        };

        if (ms.name().is_exact())
        {
            // Name does not have glob so we can use it as index into packages with exact name.
            auto name_id = pool.add_string(ms.name().str());
            pool.for_each_whatprovides(name_id, add_pkg_if_matching);
        }
        else
        {
            // Name is a Glob (e.g. ``py*``) so we have to loop through all packages.
            pool.for_each_solvable(add_pkg_if_matching);
        }
        return pool.add_to_whatprovides_data(m_packages);
    }

    auto Matcher::get_matching_packages(  //
        solv::ObjPoolView pool,
        std::string_view dep,
        const MatchFlags& flags
    ) -> solv::OffsetId
    {
        return specs::MatchSpec::parse(dep)
            .transform([&](const specs::MatchSpec& ms)
                       { return get_matching_packages(pool, ms, flags); })
            .or_else(
                [&](const auto& error) -> specs::expected_parse_t<solv::OffsetId>
                {
                    pool.set_current_error(error.what());
                    return pool.add_to_whatprovides_data({});
                }
            )
            .value();
    }

    namespace
    {
        template <typename Map>
        [[nodiscard]] auto make_cached_version(Map& cache, std::string version)
            -> specs::expected_parse_t<std::reference_wrapper<const specs::Version>>
        {
            if (auto it = cache.find(version); it != cache.cend())
            {
                return { std::cref(it->second) };
            }
            if (version.empty())
            {
                auto [it, inserted] = cache.emplace(std::move(version), specs::Version());
                return { std::cref(it->second) };
            }
            return specs::Version::parse(version).transform(
                [&](specs::Version&& ver) -> std::reference_wrapper<const specs::Version>
                {
                    auto [it, inserted] = cache.emplace(std::move(version), std::move(ver));
                    return { std::cref(it->second) };
                }
            );
        }
    }

    auto Matcher::get_pkg_attributes(solv::ObjPoolView pool, solv::ObjSolvableViewConst solv)
        -> expected_t<Pkg>
    {
        auto track_features = specs::MatchSpec::string_set();
        for (solv::StringId id : solv.track_features())
        {
            track_features.insert(std::string(pool.get_string(id)));
        }

        return make_cached_version(m_version_cache, std::string(solv.version()))
            .transform(
                [&](auto ver_ref)
                {
                    return Pkg{
                        /* .name= */ solv.name(),
                        /* .version= */ ver_ref,
                        /* .build_string= */ solv.build_string(),
                        /* .build_number= */ solv.build_number(),
                        /* .md5= */ solv.md5(),
                        /* .sha256= */ solv.sha256(),
                        /* .license= */ solv.license(),
                        /* .platform= */ std::string(solv.platform()),
                        /* .track_features= */ std::move(track_features),
                    };
                }
            )
            .transform_error(  //
                [](specs::ParseError&& err)
                { return mamba_error(err.what(), mamba_error_code::invalid_spec); }

            );
    }

    auto Matcher::pkg_match(  //
        solv::ObjPoolView pool,
        solv::ObjSolvableViewConst solv,
        const specs::MatchSpec& ms
    ) -> bool
    {
        return get_pkg_attributes(pool, solv)
            .transform([&](const Pkg& pkg) -> bool { return ms.contains_except_channel(pkg); })
            .or_else([](const auto&) -> expected_t<bool> { return false; })
            .value();
    }
}
