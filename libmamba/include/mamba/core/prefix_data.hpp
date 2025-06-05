// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PREFIX_DATA_HPP
#define MAMBA_CORE_PREFIX_DATA_HPP

#include <map>
#include <string>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/history.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba
{
    class ChannelContext;

    class PrefixData
    {
    public:

        using package_map = std::map<std::string, specs::PackageInfo>;

        static expected_t<PrefixData>
        create(const fs::u8path& prefix_path, ChannelContext& channel_context, bool no_pip = false);

        void add_packages(const std::vector<specs::PackageInfo>& packages);
        void load_single_record(const fs::u8path& path);

        const package_map& records() const;
        const package_map& pip_records() const;
        package_map all_pkg_mgr_records() const;

        History& history();
        const fs::u8path& path() const;
        std::vector<specs::PackageInfo> sorted_records() const;

        ChannelContext& channel_context() const
        {
            return m_channel_context;
        }

    private:

        PrefixData(const fs::u8path& prefix_path, ChannelContext& channel_context, bool no_pip);

        void load_site_packages();

        History m_history;
        package_map m_package_records;
        package_map m_pip_package_records;
        fs::u8path m_prefix_path;

        ChannelContext& m_channel_context;
    };
}  // namespace mamba

#endif
