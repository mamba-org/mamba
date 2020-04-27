#ifndef MAMBA_PREFIX_DATA_HPP
#define MAMBA_PREFIX_DATA_HPP

#include <unordered_map>

#include "nlohmann/json.hpp"

#include "util.hpp"

namespace mamba
{
    class  PackageRecord
    {
    public:

        PackageRecord(nlohmann::json&& j);

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
        using package_map = std::unordered_map<std::string, PackageRecord>;

        PrefixData(const std::string& prefix_path);

        void load();
        const package_map& records() const;
        void load_single_record(const fs::path& path);

        std::unordered_map<std::string, PackageRecord> m_package_records;
        fs::path m_prefix_path;
    };
}

#endif
