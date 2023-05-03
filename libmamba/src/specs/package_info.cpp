// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <nlohmann/json.hpp>

#include "mamba/specs/package_info.hpp"

namespace mamba::specs
{
    /**
     * Serialize to JSON accroding to the "repodata_record.json" format.
     */
    void to_json(nlohmann::json& j, const PackageInfo& p)
    {
        to_json(j, p.pkg);
        j["fn"] = p.file_name;
        j["url"] = p.url;
        j["channel"] = p.channel;
    }

    /**
     * Deserialize to JSON accroding to the "repodata_record.json" format.
     */
    void from_json(const nlohmann::json& j, PackageInfo& p)
    {
        from_json(j, p.pkg);
        p.file_name = j.at("fn");
        p.url = j.at("url");
        p.channel = j.at("channel");
    }
}
