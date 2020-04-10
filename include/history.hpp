#include <vector>
#include <regex>
#include <filesystem>
#include <set>

#include "thirdparty/minilog.hpp"
#include "prefix_data.hpp"
#include "match_spec.hpp"

// #include "path.hpp"
namespace fs = std::filesystem;


namespace mamba
{

class History
{
public:

    History(const std::shared_ptr<PrefixData>& prefix)
        : m_prefix_data(prefix)
    {
    }

    struct ParseResult
    {
        std::string head_line;
        std::set<std::string> diff;
        std::vector<std::string> comments;
    };

    auto parse()
    {
        std::vector<ParseResult> res;
        fs::path history_file_path = m_prefix_data->meta_dir() / "history";
        LOG(INFO) << "parsing history: " << history_file_path;

        if (!fs::exists(history_file_path))
        {
            // return empty
            return res;
        }

        std::regex head_re("==>\\s*(.+?)\\s*<==");
        std::ifstream in_file(history_file_path);
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

    struct UserRequest
    {
        std::string cmd;
        std::string conda_version;
        std::string date;

        std::vector<std::string> link_dists;
        std::vector<std::string> unlink_dists;

        std::vector<std::string> update;
        std::vector<std::string> remove;
        std::vector<std::string> neutered;
    };

    bool parse_comment_line(const std::string& line, UserRequest& req)
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
            while (std::regex_search(text_iter, (const char*) elems.c_str() + elems.size(), ematch, elems_pat))
            {
                pkg_specs.push_back(ematch[1].str());
                text_iter += ematch[0].length();
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

    auto get_user_requests()
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

    std::unordered_map<std::string, MatchSpec> get_requested_specs_map()
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

        auto& current_records = m_prefix_data->records();
        for (auto it = map.begin(); it != map.end();)
        {
            if (current_records.find(it->first) == current_records.end())
            {
                LOG(INFO) << it->first << " not installed, removing from specs";
                it = map.erase(it);
            }
            else
            {
                it++;
            }
        }
        return map;
    }

    std::shared_ptr<PrefixData> m_prefix_data;
};

}