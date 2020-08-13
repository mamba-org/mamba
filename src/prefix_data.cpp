// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/prefix_data.hpp"
#include "mamba/output.hpp"

namespace mamba
{
    PrefixData::PrefixData(const std::string& prefix_path)
        : m_history(prefix_path)
        , m_prefix_path(fs::path(prefix_path))
    {
    }

    void PrefixData::load()
    {
        auto conda_meta_dir = m_prefix_path / "conda-meta";
        if (lexists(conda_meta_dir))
        {
            for (auto& p : fs::directory_iterator(conda_meta_dir))
            {
                if (ends_with(p.path().c_str(), ".json"))
                {
                    load_single_record(p.path());
                }
            }
        }
    }

    const PrefixData::package_map& PrefixData::records() const
    {
        return m_package_records;
    }

    History& PrefixData::history()
    {
        return m_history;
    }

    const fs::path& PrefixData::path() const
    {
        return m_prefix_path;
    }

    void PrefixData::load_single_record(const fs::path& path)
    {
        LOG_INFO << "Loading single package record: " << path;
        std::ifstream infile(path);
        nlohmann::json j;
        infile >> j;
        auto prec = PackageInfo(std::move(j));
        m_package_records.insert({ prec.name, std::move(prec) });
    }
}  // namespace mamba
