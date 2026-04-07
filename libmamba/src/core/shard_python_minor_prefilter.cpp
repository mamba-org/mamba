// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>
#include <string>
#include <string_view>

#include "core/shard_python_minor_prefilter.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/specs/version_spec.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    auto version_from_single_equality_spec(const specs::VersionSpec& vs)
        -> std::optional<specs::Version>
    {
        if (vs.expression_size() != 1)
        {
            return std::nullopt;
        }
        const std::string s = vs.to_string();
        if (!util::starts_with(s, specs::VersionSpec::equal_str))
        {
            return std::nullopt;
        }
        const auto tail = std::string_view(s).substr(specs::VersionSpec::equal_str.size());
        auto maybe_v = specs::Version::parse(std::string(util::lstrip(tail)));
        if (maybe_v.has_value())
        {
            return maybe_v.value();
        }
        return std::nullopt;
    }

    auto relax_version_spec_to_minor(const specs::VersionSpec& vs) -> specs::VersionSpec
    {
        // Only relax a single exact-equality leaf; other shapes keep normal ``contains``.
        if (vs.expression_size() != 1)
        {
            return vs;
        }
        const std::string vs_str = vs.to_string();
        if (!util::starts_with(vs_str, specs::VersionSpec::equal_str))
        {
            return vs;
        }
        const auto ver_tail = std::string_view(vs_str).substr(specs::VersionSpec::equal_str.size());
        auto maybe_v = specs::Version::parse(util::lstrip(ver_tail));
        if (!maybe_v.has_value())
        {
            return vs;
        }
        const std::string minor_str = maybe_v->to_string(2);
        if (auto maybe_minor = specs::Version::parse(minor_str); maybe_minor.has_value())
        {
            return specs::VersionSpec::from_predicate(
                specs::VersionPredicate::make_equal_to(std::move(maybe_minor).value())
            );
        }
        return vs;
    }

    auto dependency_matches_python_minor_version_for_prefilter(
        const std::string& dependency_spec,
        const specs::Version& python_minor_version_for_prefilter
    ) -> bool
    {
        auto maybe_name = specs::MatchSpec::extract_name(dependency_spec);
        if (!maybe_name.has_value() || maybe_name.value() != "python")
        {
            return true;
        }
        auto maybe_match_spec = specs::MatchSpec::parse(dependency_spec);
        if (!maybe_match_spec.has_value())
        {
            return true;
        }
        const auto& ms = maybe_match_spec.value();
        const auto& vs = ms.version();
        if (vs.contains(python_minor_version_for_prefilter))
        {
            return true;
        }

        // Relax the version spec on the minor version (ignoring the patch version and build string)
        return relax_version_spec_to_minor(vs).contains(python_minor_version_for_prefilter);
    }
}
