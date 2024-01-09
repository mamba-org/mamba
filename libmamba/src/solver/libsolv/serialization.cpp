// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <nlohmann/json.hpp>

#include "mamba/solver/libsolv/serialization.hpp"
#include "mamba/util/string.hpp"

namespace mamba::solver::libsolv
{

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
