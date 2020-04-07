#pragma once

#include <filesystem>
#include <unordered_map>

#include "nlohmann/json.hpp"

#include "util.hpp"
#include "path.hpp"

namespace fs = std::filesystem;

namespace mamba
{
    class  PackageRecord
    {
    public:

        PackageRecord(const nlohmann::json&& j)
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

        std::string name;
        std::string version;
        std::string build;
        std::size_t build_number;

        std::string channel;
        std::string subdir;
        std::string fn;

        nlohmann::json json;
    };

    class PrefixData
    {
    public:
        // PrefixData(const fs::path& prefix_path)
        //     : m_prefix_path(prefix_path)
        // {
        // }

        // constructor for Python
        PrefixData(const std::string& prefix_path)
            : m_prefix_path(fs::path(prefix_path))
        {
        }

        void load()
        {
            auto conda_meta_dir = m_prefix_path / "conda-meta";
            if (path::lexists(conda_meta_dir))
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

        const std::unordered_map<std::string, PackageRecord>& records() const
        {
            return m_package_records;
        }

        void load_single_record(const fs::path& path)
        {
            LOG(INFO) << "Loading single package record: " << path;
            std::ifstream infile(path);
            nlohmann::json j;
            infile >> j;
            auto prec = PackageRecord(std::move(j));
            m_package_records.insert({prec.name, std::move(prec)});
        }

        std::unordered_map<std::string, PackageRecord> m_package_records;
        fs::path m_prefix_path;
    };

}