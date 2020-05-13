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
