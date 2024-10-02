// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/history.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    History::History(const fs::u8path& prefix, ChannelContext& channel_context)
        : m_prefix(prefix)
        , m_history_file_path(fs::absolute(m_prefix / "conda-meta" / "history"))
        , m_channel_context(channel_context)
    {
    }

    History::UserRequest History::UserRequest::prefilled(const Context& context)
    {
        UserRequest ur;
        std::time_t t = std::time(nullptr);
        char mbstr[100];
        if (std::strftime(mbstr, sizeof(mbstr), "%Y-%m-%d %H:%M:%S", std::localtime(&t)))
        {
            ur.date = mbstr;
        }
        ur.cmd = context.command_params.current_command;
        ur.conda_version = context.command_params.conda_version;
        return ur;
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

        static const std::regex head_re("==>\\s*(.+?)\\s*<==");
        std::ifstream in_file = open_ifstream(m_history_file_path, std::ios::in);

        std::string line;
        while (getline(in_file, line))
        {
            if (line.size() == 0)
            {
                continue;
            }
            std::smatch base_match;
            if (std::regex_match(line, base_match, head_re))
            {
                ParseResult p;
                p.head_line = base_match[1].str();
                res.push_back(p);
            }
            else if (line[0] == '#')
            {
                if (res.size() > 0)
                {
                    res[res.size() - 1].comments.push_back(line);
                }
                else
                {
                    res.push_back(ParseResult());
                    res[0].comments.push_back(line);
                }
            }
            else if (line.size() != 0)
            {
                if (res.size() > 0)
                {
                    res[res.size() - 1].diff.insert(line);
                }
                else
                {
                    res.push_back(ParseResult());
                    res[0].diff.insert(line);
                }
            }
        }
        return res;
    }

    bool History::parse_comment_line(const std::string& line, UserRequest& req)
    {
        std::size_t colon_idx = line.find_first_of(':');
        if (colon_idx == std::string::npos)
        {
            return false;
        }

        std::string key(util::strip(line.substr(1, colon_idx - 1)));
        std::string value(util::strip(line.substr(colon_idx + 1)));

        if (key == "conda version")
        {
            req.conda_version = value;
        }
        else if (key == "cmd")
        {
            req.cmd = value;
        }
        else if (util::ends_with(key, " specs"))
        {
            std::string action = key.substr(0, key.find_first_of(" "));
            // small parser for pythonic lists
            std::vector<std::string> pkg_specs;
            std::size_t idx_start = value.find_first_of("\'\"");
            std::size_t idx_end, idx_search;
            idx_search = idx_start + 1;
            std::string needle = "X";

            while (true)
            {
                needle[0] = value[idx_start];
                idx_end = value.find_first_of(needle.c_str(), idx_search);

                // Capturing `MatchSpecs` without internal quotes (e.g `libcurl`)
                if (idx_end != std::string::npos && value[idx_end - 1] != '\\')
                {
                    auto start_string = idx_start + 1;
                    auto len_matchspec = idx_end - start_string;

                    // Quotes are excluded (e.g. `libcurl` is extracted from `"libcurl"`)
                    auto match_spec = value.substr(start_string, len_matchspec);
                    pkg_specs.push_back(match_spec);
                    idx_start = value.find_first_of("\'\"", idx_end + 1);
                    idx_search = idx_start + 1;
                }
                // Capturing `MatchSpecs` with metadata (e.g `libcurl[version=\">=7.86,<8.10\"]`)
                else if (idx_end != std::string::npos && value[idx_end - 1] == '\\')
                {
                    // Find if "[" is present in between idx_search and idx_end
                    auto idx_bracket = value.find_first_of("[", idx_search);

                    // If "[" is present, then find the closing bracket
                    if (idx_bracket != std::string::npos && idx_bracket < idx_end)
                    {
                        auto idx_closing_bracket = value.find_first_of("]", idx_bracket);
                        if (idx_closing_bracket != std::string::npos)
                        {
                            auto start_string = idx_start + 1;
                            auto end_string = idx_closing_bracket + 1;
                            auto len_matchspec = end_string - start_string;

                            // Quotes are excluded (e.g. `libcurl[version=\">=7.86,<8.10\"]` is
                            // extracted from `"libcurl[version=\">=7.86,<8.10\"]"`)
                            auto match_spec = value.substr(start_string, len_matchspec);
                            // Remove the backslash from the MatchSpec
                            match_spec.erase(
                                std::remove(match_spec.begin(), match_spec.end(), '\\'),
                                match_spec.end()
                            );
                            pkg_specs.push_back(match_spec);
                            idx_start = value.find_first_of("\'\"", end_string + 1);
                            idx_search = idx_start + 1;
                        }
                    }
                    // If "[" is not present, then there's a problem with the MatchSpec
                    else if (idx_bracket == std::string::npos || idx_bracket > idx_end)
                    {
                        throw std::runtime_error("Parsing of history file failed at: " + value);
                    }
                }

                else
                {
                    idx_search = idx_end;
                }
                if (idx_start >= value.size() || idx_start == std::string::npos)
                {
                    break;
                }
                if (idx_search >= value.size() || idx_search == std::string::npos)
                {
                    throw std::runtime_error("Parsing of history file failed");
                }
            }

            if (action == "update" || action == "install" || action == "create")
            {
                req.update = pkg_specs;
            }
            else if (action == "remove" || action == "uninstall")
            {
                req.remove = pkg_specs;
            }
            else if (action == "neutered")
            {
                req.neutered = pkg_specs;
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

    std::unordered_map<std::string, specs::MatchSpec> History::get_requested_specs_map()
    {
        std::unordered_map<std::string, specs::MatchSpec> map;

        auto to_specs = [&](const std::vector<std::string>& sv)
        {
            std::vector<specs::MatchSpec> v;
            v.reserve(sv.size());
            for (const auto& el : sv)
            {
                v.emplace_back(specs::MatchSpec::parse(el)
                                   .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                   .value());
            }
            return v;
        };

        for (const auto& request : get_user_requests())
        {
            auto remove_specs = to_specs(request.remove);
            for (auto& spec : remove_specs)
            {
                map.erase(spec.name().str());
            }
            auto update_specs = to_specs(request.update);
            for (auto& spec : update_specs)
            {
                map[spec.name().str()] = spec;
            }
            auto neutered_specs = to_specs(request.neutered);
            for (auto& spec : neutered_specs)
            {
                map[spec.name().str()] = spec;
            }
        }

        return map;
    }

    void History::add_entry(const History::UserRequest& entry)
    {
        LOG_INFO << "Opening history file: " << m_history_file_path;
        if (!fs::exists(m_history_file_path))
        {
            path::touch(m_history_file_path);
        }
        std::ofstream out = open_ofstream(m_history_file_path, std::ios::app);

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
                                   const std::vector<std::string>& specs) -> std::string
            {
                if (specs.empty())
                {
                    return "";
                }
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
