// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_HISTORY
#define MAMBA_HISTORY

#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "match_spec.hpp"
#include "output.hpp"
#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{
    class History
    {
    public:
        History(const std::string& prefix);

        struct ParseResult
        {
            std::string head_line;
            std::set<std::string> diff;
            std::vector<std::string> comments;
        };

        struct UserRequest
        {
            static UserRequest prefilled()
            {
                UserRequest ur;
                std::time_t t = std::time(nullptr);
                char mbstr[100];
                if (std::strftime(mbstr, sizeof(mbstr), "%Y-%m-%d %H:%M:%S", std::localtime(&t)))
                {
                    ur.date = mbstr;
                }
                ur.cmd = Context::instance().current_command;
                ur.conda_version = Context::instance().conda_version;
                return ur;
            }

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
        void add_entry(const History::UserRequest& entry);

        std::string m_prefix;
        fs::path m_history_file_path;
    };

}  // namespace mamba

#endif
