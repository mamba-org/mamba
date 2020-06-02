#ifndef MAMBA_PACKAGE_INFO
#define MAMBA_PACKAGE_INFO

#include <string>

extern "C" {
    #include <solv/solvable.h>
    #include <solv/pool.h>
    #include <solv/repo.h>
    #include <solv/poolid.h>
}

#include "nlohmann/json.hpp"

namespace mamba
{
    class PackageInfo
    {
    public:

        using field_getter = std::function<std::string (const PackageInfo&)>;
        using compare_fun = std::function<bool (const PackageInfo&, const PackageInfo&)>;

        static field_getter get_field_getter(const std::string& name);
        static compare_fun less(const std::string& member);
        static compare_fun equal(const std::string& member);

        PackageInfo(Solvable* s);
        PackageInfo(nlohmann::json&& j);
        PackageInfo(const std::string& name);
        PackageInfo(const std::string& name, const std::string& version,
                    const std::string build_string, std::size_t build_number);

        nlohmann::json json() const;
        std::string str() const;
        std::string long_str() const;

        std::string name;
        std::string version;
        std::string build_string;
        std::size_t build_number;
        std::string channel;
        std::string url;
        std::string subdir;
        std::string fn;
        std::string license;
        std::size_t size;
        std::size_t timestamp;
        std::string md5;
        std::string sha256;
        std::vector<std::string> depends;
        std::vector<std::string> constrains;
    };
}

#endif
