#ifndef MAMBA_MATCH_SPEC
#define MAMBA_MATCH_SPEC

#include <regex>
#include <string>
#include <unordered_map>

#include "nlohmann/json.hpp"

extern "C" {
    #include <solv/solvable.h>
    #include <solv/pool.h>
    #include <solv/repo.h>
    #include <solv/poolid.h>
}

namespace mamba
{
    class PackageInfo
    {
    public:

        PackageInfo(Solvable* s);
        PackageInfo(nlohmann::json&& j);
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

    class MatchSpec
    {
    public:

        MatchSpec() = default;
        MatchSpec(const std::string& i_spec);

        void parse();
        std::string triple() const;
        std::string conda_build_form() const;
        std::string str() const;

        static std::tuple<std::string, std::string> parse_version_and_build(const std::string& s);
        std::string spec;

        std::string name;
        std::string version;
        std::string channel;
        std::string ns;
        std::string subdir;
        std::string build;
        std::string fn;
        std::string url;
        std::string build_number;

        bool is_file = false;
        bool optional = false;
        std::unordered_map<std::string, std::string> brackets;
        std::unordered_map<std::string, std::string> parens;
    };
}

#endif 
