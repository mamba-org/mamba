#include "history.hpp"
#include <fstream>

namespace mamba
{

    History::History(const std::string& prefix)
        : m_prefix_data(prefix)
    {
    }

    std::vector<History::ParseResult> History::parse()
    {
        std::vector<ParseResult> res;
        m_history_file_path = fs::path(m_prefix_data) / "conda-meta" / "history";
        LOG_INFO << "parsing history: " << m_history_file_path;

        if (!fs::exists(m_history_file_path))
        {
            // return empty
            return res;
        }

        std::regex head_re("==>\\s*(.+?)\\s*<==");
        std::ifstream in_file(m_history_file_path);
        std::string line;
        while (getline(in_file, line))
        {
            // line.strip()
            if (line.size() == 0) continue;
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
        std::regex com_pat("#\\s*cmd:\\s*(.+)");
        std::regex conda_v_pat("#\\s*conda version:\\s*(.+)");
        std::regex spec_pat("#\\s*(\\w+)\\s*specs:\\s*(.+)?");
        std::regex elems_pat("'([^',]+)'");
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
            const char* text_iter = elems.c_str();
            while (std::regex_search(text_iter, ematch, elems_pat))
            {
                pkg_specs.push_back(ematch[1].str());
                text_iter += ematch.position() + ematch.length();
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
                    r.unlink_dists.push_back(x);
                }
                else if (x[0] == '+')
                {
                    r.link_dists.push_back(x);
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
        // auto& current_records = m_prefix_data->records();
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

    void History::add_entry(const std::vector<History::UserRequest> user_request)
    {
        std::ofstream out;
        out.open(m_history_file_path, std::ios_base::app);
        if (out.fail())
        {
            throw std::runtime_error("Couldn't open file: " + m_history_file_path.string());
        }
        else
        {
            for (auto request : user_request)
            {
                out << "==> " << request.date << " <==" << std::endl;
                out << "# cmd: " << request.cmd << std::endl;
                out << "# conda version: " << request.conda_version << std::endl;
                for (auto unlink_dist : request.unlink_dists)
                {
                    out << unlink_dist << std::endl;
                }
                for (auto link_dist : request.link_dists)
                {
                    out << link_dist << std::endl;
                }


                auto specs_output = [](const std::string& action, const std::vector<std::string>& specs) -> std::string {
                    std::string spec_string;
                    spec_string ="# " + action + " specs: [";
                    for (auto spec : specs)
                    {
                        spec_string += "'" + spec + "',";
                    }
                    spec_string.back() = ']';
                    spec_string += '\n';
                    return spec_string;
                };

                if (request.update.size() > 0)
                {
                    out << specs_output("update", request.update);
                }
                if (request.remove.size() > 0)
                {
                    out << specs_output("remove", request.remove);
                }
                if (request.neutered.size() > 0)
                {
                    out << specs_output("neutered", request.neutered);
                }
            }
        }
    }
}