// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <tuple>

#include <nlohmann/json.hpp>

#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/util/string.hpp"

namespace mamba::solver::libsolv
{
    namespace
    {
        auto attrs(const Priorities& p)
        {
            return std::tie(p.priority, p.subpriority);
        }
    }

    auto operator==(const Priorities& lhs, const Priorities& rhs) -> bool
    {
        return attrs(lhs) == attrs(rhs);
    }

    auto operator!=(const Priorities& lhs, const Priorities& rhs) -> bool
    {
        return !(lhs == rhs);
    }

    auto operator==(const RepodataOrigin& lhs, const RepodataOrigin& rhs) -> bool
    {
        return (util::rstrip(lhs.url, '/') == util::rstrip(rhs.url, '/')) && (lhs.etag == rhs.etag)
               && (lhs.mod == rhs.mod);
    }

    auto operator!=(const RepodataOrigin& lhs, const RepodataOrigin& rhs) -> bool
    {
        return !(lhs == rhs);
    }

    void to_json(nlohmann::json& j, const RepodataOrigin& m)
    {
        j["url"] = m.url;
        j["etag"] = m.etag;
        j["mod"] = m.mod;
    }

    void from_json(const nlohmann::json& j, RepodataOrigin& p)
    {
        p.url = j.value("url", "");
        p.etag = j.value("etag", "");
        p.mod = j.value("mod", "");
    }
}
