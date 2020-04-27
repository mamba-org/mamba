#ifndef MAMBA_MATCH_SPEC
#define MAMBA_MATCH_SPEC

#include <regex>
#include <string>
#include <unordered_map>

namespace mamba
{
    class MatchSpec
    {
    public:

        MatchSpec() = default;
        MatchSpec(const std::string& i_spec);

        void parse();

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
