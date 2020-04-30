#ifndef MAMBA_HISTORY
#define MAMBA_HISTORY

#include <vector>
#include <regex>
#include <set>

#include "thirdparty/filesystem.hpp"

#include "output.hpp"
// #include "prefix_data.hpp"
#include "match_spec.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{

class History
{
public:

    // History(const std::shared_ptr<PrefixData>& prefix)
    //     : m_prefix_data(prefix)
    // {
    // }

    History(const std::string& prefix);

    struct ParseResult
    {
        std::string head_line;
        std::set<std::string> diff;
        std::vector<std::string> comments;
    };

    struct UserRequest
    {
        std::string date;
        std::string cmd;
        std::string conda_version;

        std::vector<std::string> unlink_dists;
        std::vector<std::string> link_dists;

        std::vector<std::string> update;
        std::vector<std::string> remove;
        std::vector<std::string> neutered;
    };

    std::vector<ParseResult> parse();
    bool parse_comment_line(const std::string& line, UserRequest& req);
    std::vector<UserRequest> get_user_requests();
    std::unordered_map<std::string, MatchSpec> get_requested_specs_map();
    void add_entry(const std::vector<History::UserRequest> user_request);

    // std::shared_ptr<PrefixData> m_prefix_data;
    std::string m_prefix_data;
    fs::path m_history_file_path;
};

}

#endif