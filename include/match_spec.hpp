#ifndef MAMBA_MATCH_SPEC
#define MAMBA_MATCH_SPEC

#include <regex>
#include <string>
#include <unordered_map>

#include "url.hpp"
#include "thirdparty/pystring14/pystring.hpp"

namespace mamba
{
    class MatchSpec
    {
    public:

        MatchSpec(const std::string& i_spec);
        MatchSpec() = default;

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