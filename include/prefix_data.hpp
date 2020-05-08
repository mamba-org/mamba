#ifndef MAMBA_PREFIX_DATA_HPP
#define MAMBA_PREFIX_DATA_HPP

#include <unordered_map>

#include "nlohmann/json.hpp"

#include "history.hpp"
#include "util.hpp"

namespace mamba
{
    class PrefixData
    {
    public:
        using package_map = std::unordered_map<std::string, PackageInfo>;

        PrefixData(const std::string& prefix_path);

        void load();
        const package_map& records() const;
        void load_single_record(const fs::path& path);

        History& history();
        const fs::path& path() const;

        History m_history;
        std::unordered_map<std::string, PackageInfo> m_package_records;
        fs::path m_prefix_path;
    };
}

#endif
