// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PACKAGE_INFO
#define MAMBA_CORE_PACKAGE_INFO

extern "C"
{
#include <solv/pool.h>
#include <solv/poolid.h>
#include <solv/repo.h>
#include <solv/solvable.h>
}

#include "util.hpp"
#include <string>
#include <vector>
#include <set>


#include "nlohmann/json.hpp"

namespace mamba
{
    class PackageInfo
    {
    public:
        using field_getter = std::function<std::string(const PackageInfo&)>;
        using compare_fun = std::function<bool(const PackageInfo&, const PackageInfo&)>;

        static field_getter get_field_getter(const std::string& name);
        static compare_fun less(const std::string& member);
        static compare_fun equal(const std::string& member);

        PackageInfo(Solvable* s);
        PackageInfo(nlohmann::json&& j);
        PackageInfo(const std::string& name);
        PackageInfo(const std::string& name,
                    const std::string& version,
                    const std::string build_string,
                    std::size_t build_number);

        nlohmann::json json_record() const;
        nlohmann::json json_signable() const;
        std::string str() const;
        std::string long_str() const;

        std::string name;
        std::string version;
        std::string build_string;
        std::string noarch;
        std::size_t build_number = 0;
        std::string channel;
        std::string url;
        std::string subdir;
        std::string fn;
        std::string license;
        std::size_t size = 0;
        std::size_t timestamp = 0;
        std::string md5;
        std::string sha256;
        std::string track_features;
        std::vector<std::string> depends;
        std::vector<std::string> constrains;
        std::string signatures;
        std::string extra_metadata;
        std::set<std::string> defaulted_keys;
    };

    inline bool operator==(const PackageInfo& package_info_a, const PackageInfo& package_info_b)
    {
        return package_info_a.name == package_info_b.name
               && package_info_a.version == package_info_b.version
               && package_info_a.build_string == package_info_b.build_string
               && package_info_a.noarch == package_info_b.noarch
               && package_info_a.build_number == package_info_b.build_number
               && package_info_a.channel == package_info_b.channel
               && package_info_a.url == package_info_b.url
               && package_info_a.subdir == package_info_b.subdir
               && package_info_a.fn == package_info_b.fn
               && package_info_a.license == package_info_b.license
               && package_info_a.size == package_info_b.size
               && package_info_a.timestamp == package_info_b.timestamp
               && package_info_a.md5 == package_info_b.md5
               && package_info_a.sha256 == package_info_b.sha256
               && package_info_a.track_features == package_info_b.track_features
               && package_info_a.depends == package_info_b.depends
               && package_info_a.constrains == package_info_b.constrains
               && package_info_a.signatures == package_info_b.signatures
               && package_info_a.extra_metadata == package_info_b.extra_metadata
               && package_info_a.defaulted_keys == package_info_b.defaulted_keys;
    }

    struct HashFunction
    {
        size_t operator()(const PackageInfo& package_info) const
        {
            std::vector<std::string> entries{
                package_info.name,   package_info.version, package_info.build_string,
                package_info.noarch, package_info.channel, package_info.url,
                package_info.md5,    package_info.sha256,  package_info.subdir,
            };
            std::vector<size_t> v = to_size_t(entries);
            v.push_back(std::hash<size_t>{}(package_info.build_number));

            size_t depends_hash = hash<size_t>(to_size_t(package_info.depends));
            size_t constrains_hash = hash<size_t>(to_size_t(package_info.constrains));
            v.push_back(depends_hash);
            v.push_back(constrains_hash);

            return std::hash<std::string>{}(package_info.subdir);
        }

        std::vector<size_t> to_size_t(const std::vector<std::string>& entries) const
        {
            std::vector<size_t> v;
            std::transform(entries.begin(),
                           entries.end(),
                           std::back_inserter(v),
                           [](auto entry) { return std::hash<std::string>{}(entry); });
            return v;
        }
    };
}  // namespace mamba

#endif
