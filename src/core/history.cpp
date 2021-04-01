// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <fstream>

#include "mamba/core/fsutil.hpp"
#include "mamba/core/history.hpp"

namespace mamba
{
    History::History(const std::string& prefix)
        : m_prefix(prefix)
        , m_history_file_path(fs::path(m_prefix) / "conda-meta" / "history")
    {
    }

    std::vector<History::ParseResult> History::parse()
    {
        std::vector<ParseResult> res;
        LOG_INFO << "parsing history: " << m_history_file_path;

        if (!fs::exists(m_history_file_path))
        {
            // return empty
            return res;
        }

        static std::regex head_re("==>\\s*(.+?)\\s*<==");
        std::ifstream in_file(m_history_file_path);
        std::string line;
        while (getline(in_file, line))
        {
            // line.strip()
            if (line.size() == 0)
                continue;
            std::smatch base_match;
            if (std::regex_match(line, base_match, head_re))
            {
                ParseResult p;
                p.head_line = base_match[1].str();
                res.push_back(std::move(p));
            }
            else if (line[0] == '#')
            {
                res[res.size() - 1].comments.push_back(line);
            }
            else if (line.size() != 0)
            {
                res[res.size() - 1].diff.insert(line);
            }
        }
        return res;
    }

    bool History::parse_comment_line(const std::string& line, UserRequest& req)
    {
        static std::regex com_pat("#\\s*cmd:\\s*(.+)");
        static std::regex conda_v_pat("#\\s*conda version:\\s*(.+)");
        static std::regex spec_pat("#\\s*(\\w+)\\s*specs:\\s*(.+)?");

        std::smatch rmatch;

        if (std::regex_match(line, rmatch, com_pat))
        {
            req.cmd = rmatch[1].str();
        }
        else if (std::regex_match(line, rmatch, conda_v_pat))
        {
            req.conda_version = rmatch[1].str();
        }
        else if (std::regex_match(line, rmatch, spec_pat))
        {
            std::string action = rmatch[1].str();
            std::string elems = rmatch[2].str();

            std::cmatch ematch;
            std::vector<std::string> pkg_specs;

            // small parser for pythonic lists
            std::size_t idx_start = elems.find_first_of("\'\"");
            std::size_t idx_end, idx_search;
            idx_search = idx_start + 1;
            std::string needle = "X";

            while (true)
            {
                needle[0] = elems[idx_start];
                idx_end = elems.find_first_of(needle.c_str(), idx_search);
                if (idx_end != std::string::npos && elems[idx_end - 1] != '\\')
                {
                    pkg_specs.push_back(elems.substr(idx_start + 1, idx_end - 1 - idx_start));
                    idx_start = elems.find_first_of("\'\"", idx_end + 1);
                    idx_search = idx_start + 1;
                }
                else
                {
                    idx_search = idx_end;
                }
                if (idx_start >= elems.size() || idx_start == std::string::npos)
                {
                    break;
                }
                if (idx_search >= elems.size() || idx_search == std::string::npos)
                {
                    throw std::runtime_error("Parsing of history file failed");
                }
                // pkg_specs.push_back(ematch[1].str());
                // text_iter += ematch.position() + ematch.length();
            }
            if (action == "update" || action == "install" || action == "create")
            {
                req.update = std::move(pkg_specs);
            }
            else if (action == "remove" || action == "uninstall")
            {
                req.remove = std::move(pkg_specs);
            }
            else if (action == "neutered")
            {
                req.neutered = std::move(pkg_specs);
            }
        }
        return true;
    }

    std::vector<History::UserRequest> History::get_user_requests()
    {
        std::vector<UserRequest> res;
        for (const auto& el : parse())
        {
            UserRequest r;
            r.date = el.head_line;
            for (const auto& c : el.comments)
            {
                parse_comment_line(c, r);
            }

            for (const auto& x : el.diff)
            {
                if (x[0] == '-')
                {
                    r.unlink_dists.push_back(x.substr(1));
                }
                else if (x[0] == '+')
                {
                    r.link_dists.push_back(x.substr(1));
                }
            }
            res.push_back(r);
        }
        // TODO add some stuff here regarding version of conda?
        return res;
    }

    std::unordered_map<std::string, MatchSpec> History::get_requested_specs_map()
    {
        std::unordered_map<std::string, MatchSpec> map;

        auto to_specs = [](const std::vector<std::string>& sv) {
            std::vector<MatchSpec> v;
            v.reserve(sv.size());
            for (const auto& el : sv)
            {
                v.emplace_back(el);
            }
            return v;
        };

        for (const auto& request : get_user_requests())
        {
            auto remove_specs = to_specs(request.remove);
            for (auto& spec : remove_specs)
            {
                map.erase(spec.name);
            }
            auto update_specs = to_specs(request.update);
            for (auto& spec : update_specs)
            {
                map[spec.name] = spec;
            }
            auto neutered_specs = to_specs(request.neutered);
            for (auto& spec : neutered_specs)
            {
                map[spec.name] = spec;
            }
        }

        // TODO Add this back in once we merge the PrefixData PR!
        // auto& current_records = m_prefix->records();
        // for (auto it = map.begin(); it != map.end();)
        // {
        //     if (current_records.find(it->first) == current_records.end())
        //     {
        //         LOG_INFO << it->first << " not installed, removing from specs";
        //         it = map.erase(it);
        //     }
        //     else
        //     {
        //         it++;
        //     }
        // }
        return map;
    }

    void History::add_entry(const History::UserRequest& entry)
    {
        LOG_INFO << "Opening history file: " << m_history_file_path;
        if (!fs::exists(m_history_file_path))
        {
            path::touch(m_history_file_path);
        }
        std::ofstream out(m_history_file_path, std::ios::app);

        if (out.fail())
        {
            throw std::runtime_error("Couldn't open file: " + m_history_file_path.string());
        }
        else
        {
            out << "==> " << entry.date << " <==" << std::endl;
            out << "# cmd: " << entry.cmd << std::endl;
            out << "# conda version: " << entry.conda_version << std::endl;

            for (auto unlink_dist : entry.unlink_dists)
            {
                out << "-" << unlink_dist << std::endl;
            }
            for (auto link_dist : entry.link_dists)
            {
                out << "+" << link_dist << std::endl;
            }

            auto specs_output = [](const std::string& action,
                                   const std::vector<std::string>& specs) -> std::string {
                if (specs.empty())
                    return "";
                std::stringstream spec_ss;
                spec_ss << "# " << action << " specs: [";
                for (auto spec : specs)
                {
                    spec_ss << std::quoted(spec) << ", ";
                }
                std::string spec_string(spec_ss.str());
                spec_string[spec_string.size() - 2] = ']';
                spec_string.back() = '\n';
                return spec_string;
            };

            out << specs_output("update", entry.update);
            out << specs_output("remove", entry.remove);
            out << specs_output("neutered", entry.neutered);
        }
    }
}  // namespace mamba
