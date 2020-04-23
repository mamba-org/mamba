#include "prefix_data.hpp"
#include "output.hpp"

namespace mamba
{
    PackageRecord::PackageRecord(nlohmann::json&& j)
            : json(std::move(j))
    {
        name = json["name"];
        version = json["version"];
        build = json["build"];
        build_number = json["build_number"];
        channel = json["channel"];
        subdir = json["subdir"];
        fn = json["fn"];
    }

    PrefixData::PrefixData(const std::string& prefix_path)
        : m_prefix_path(fs::path(prefix_path))
    {
    }

    void PrefixData::load()
    {
        auto conda_meta_dir = m_prefix_path / "conda-meta";
        if (lexists(conda_meta_dir))
        {
            for(auto& p: fs::directory_iterator(conda_meta_dir))
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

    void PrefixData::load_single_record(const fs::path& path)
    {
        LOG_INFO << "Loading single package record: " << path;
        std::ifstream infile(path);
        nlohmann::json j;
        infile >> j;
        auto prec = PackageRecord(std::move(j));
        m_package_records.insert({prec.name, std::move(prec)});
    }
}