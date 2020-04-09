#include <vector>
#include <regex>
#include <filesystem>
#include <set>

#include "thirdparty/minilog.hpp"

// #include "path.hpp"
namespace fs = std::filesystem;


namespace mamba
{

class History
{
public:
    // Todo instead of passing a path here, maybe we should have a
    // dedicated prefix class
    History(const fs::path& prefix)
        : m_prefix_path(prefix)
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
        std::cout << "checking path: " << (m_prefix_path / "history") << std::endl;
        if (!fs::exists(m_prefix_path / "history"))
        {
            // return empty
            return res;
        }

        std::regex head_re("==>\\s*(.+?)\\s*<==");
        std::ifstream in_file(m_prefix_path  / "history");
        std::string line;
        while (getline(in_file, line))
        {
            // std::cout << line << std::endl;
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

    struct UserRequests
    {
        std::string cmd;
        std::string conda_version;
        std::vector<std::string> update;
        std::vector<std::string> remove;
        std::vector<std::string> neutered;
    };

    void parse_comment_line(const std::string& line, UserRequests& req)
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
    }

    fs::path m_prefix_path;
};

}