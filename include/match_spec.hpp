#ifndef MAMBA_MATCH_SPEC
#define MAMBA_MATCH_SPEC

#include <regex>
#include <string>
#include <unordered_map>

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
        PackageInfo(const std::string& name, const std::string& version,
                    const std::string build, std::size_t build_number);

        std::string str() const;

        std::string name;
        std::string version;
        std::string build;
        std::size_t build_number;
    };

    class MatchSpec
    {
    public:

        MatchSpec() = default;
        MatchSpec(const std::string& i_spec);

        void parse();
        std::string triple() const;

        std::string spec;

        std::string name;
        std::string version;
        std::string channel;
        std::string ns;
        std::string subdir;
        std::string build;
        std::string fn;
        std::string url;

        std::unordered_map<std::string, std::string> brackets;
    };
}

#endif 
