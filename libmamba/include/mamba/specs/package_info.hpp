// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_PACKAGE_INFO_HPP
#define MAMBA_SPECS_PACKAGE_INFO_HPP

#include <string>

#include <nlohmann/json_fwd.hpp>

#include "mamba/specs/repo_data.hpp"

namespace mamba::specs
{
    /**
     * Information about a package from a given repodata.
     *
     * It includes a ``RepoDataRecord`` from the given repo ``repodata.json`` but it also stores
     * the source of the data (like the url and the channel).
     */
    struct PackageInfo
    {
        /** The data stored in the repodata.json``. */
        RepoDataPackage pkg = {};

        /** The filename of the package, such as ``libmamba-0.13.0-h3a044de_0.tar.bz2``. */
        std::string file_name = {};

        /**
         * The canonical URL from where to get this package.
         *
         * @todo Use a proper class for representing a Url if/when available.
         */
        std::string file_url = {};

        /**
         * String representation of the channel where the package comes from.
         *
         * This could be a channel name but preferably it should be the complete URL.
         *
         * @todo Merge ``url`` and ``channel`` with a fully detailled ``ChannelInfo``.
         */
        std::string channel = {};
    };

    /**
     * Serialize to JSON accroding to the "repodata_record.json" format.
     */
    void to_json(nlohmann::json& j, const PackageInfo& p);

    /**
     * Deserialize to JSON accroding to the "repodata_record.json" format.
     */
    void from_json(const nlohmann::json& j, PackageInfo& p);

}
#endif
