// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <list>
#include <regex>

#include "mamba/core/channel_context.hpp"
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

    History::UserRequest History::UserRequest::prefilled(const CommandParams& command_params)
    {
        UserRequest ur;
        std::time_t t = std::time(nullptr);
        char mbstr[100];
        if (std::strftime(mbstr, sizeof(mbstr), "%Y-%m-%d %H:%M:%S", std::localtime(&t)))
        {
            ur.date = mbstr;
        }
        ur.cmd = command_params.current_command;
        ur.conda_version = command_params.conda_version;
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
                    res[res.size() - 1].diff.push_back(line);
                }
                else
                {
                    res.push_back(ParseResult());
                    res[0].diff.push_back(line);
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
                    pkg_specs.push_back(value.substr(idx_start + 1, idx_end - 1 - idx_start));
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
                            pkg_specs.push_back(std::move(match_spec));
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
        std::size_t revision_num = 0;
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
            if ((r.link_dists.size() > 0) || (r.unlink_dists.size() > 0))
            {
                r.revision_num = revision_num++;
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
                map.erase(spec.name().to_string());
            }
            auto update_specs = to_specs(request.update);
            for (auto& spec : update_specs)
            {
                map[spec.name().to_string()] = spec;
            }
            auto neutered_specs = to_specs(request.neutered);
            for (auto& spec : neutered_specs)
            {
                map[spec.name().to_string()] = spec;
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

    specs::PackageInfo read_history_url_entry(const std::string& s)
    {
        std::string name_version_build_string;
        std::string channel;
        std::size_t pos_0 = s.rfind("/");
        if (pos_0 != std::string::npos)
        {
            // s is of the form of
            // `https://conda.anaconda.org/conda-forge/linux-64::xtl-0.8.0-h84d6215_0` or
            // `conda-forge/linux-64::xtl-0.8.0-h84d6215_0`
            std::string s_begin = s.substr(0, pos_0);
            // s_begin is of the form of `https://conda.anaconda.org/conda-forge` or
            // `conda-forge`
            std::string s_end = s.substr(pos_0 + 1, s.size());
            // s_end is of the form of `linux-64::xtl-0.8.0-h84d6215_0`
            std::size_t pos = s_begin.rfind("/");
            if (pos != std::string::npos)
            {
                channel = s_begin.substr(pos + 1, s_begin.size());
            }
            else
            {
                channel = s_begin;
            }
            name_version_build_string = std::get<1>(util::rsplit_once(s_end, "::"));
        }
        else
        {
            // s is of the form of `conda-forge::xtl-0.8.0-h84d6215_0`
            auto double_colon_split = util::split_once(s, "::");
            channel = std::get<0>(double_colon_split);
            name_version_build_string = std::get<1>(double_colon_split).value_or("");
        }

        // `name_version_build_string` is of the form `xtl-0.8.0-h84d6215_0`
        std::size_t pos_1 = name_version_build_string.rfind("-");
        std::string name_version = name_version_build_string.substr(0, pos_1);
        // `name_version` is of the form `xtl-0.8.0`
        std::string build_string = name_version_build_string.substr(
            pos_1 + 1,
            name_version_build_string.size()
        );

        std::size_t pos_2 = name_version.rfind("-");
        std::string name = name_version.substr(0, pos_2);
        std::string version = name_version.substr(pos_2 + 1, name_version.size());

        specs::PackageInfo pkg_info{ name, version, build_string, channel };
        return pkg_info;
    }

    PackageDiff PackageDiff::from_revision(
        const std::vector<History::UserRequest>& user_requests,
        std::size_t target_revision
    )
    {
        assert(!user_requests.empty());

        struct revision
        {
            std::size_t key = 0;
            std::unordered_map<std::string, specs::PackageInfo> removed_pkg = {};
            std::unordered_map<std::string, specs::PackageInfo> installed_pkg = {};
        };

        std::list<revision> revisions;
        for (auto r : user_requests)
        {
            if ((r.link_dists.size() > 0) || (r.unlink_dists.size() > 0))
            {
                if (r.revision_num > target_revision)
                {
                    revision rev{ /*.key = */ r.revision_num };
                    for (auto ud : r.unlink_dists)
                    {
                        auto pkg_info = read_history_url_entry(ud);
                        const auto name = pkg_info.name;
                        rev.removed_pkg[name] = std::move(pkg_info);
                    }
                    for (auto ld : r.link_dists)
                    {
                        auto pkg_info = read_history_url_entry(ld);
                        const auto name = pkg_info.name;
                        rev.installed_pkg[name] = std::move(pkg_info);
                    }
                    revisions.push_back(rev);
                }
            }
        }

        PackageDiff pkg_diff{};

        const auto handle_install = [&pkg_diff](revision& rev, const std::string& pkg_name)
        {
            bool res = false;
            if (auto rev_iter = rev.installed_pkg.find(pkg_name); rev_iter != rev.installed_pkg.end())
            {
                const auto version = rev_iter->second.version;
                auto iter = pkg_diff.removed_pkg_diff.find(pkg_name);
                if (iter != pkg_diff.removed_pkg_diff.end() && iter->second.version == version)
                {
                    pkg_diff.removed_pkg_diff.erase(iter);
                }
                else
                {
                    pkg_diff.installed_pkg_diff[pkg_name] = rev_iter->second;
                }
                rev.installed_pkg.erase(rev_iter);
                res = true;
            }
            return res;
        };

        auto handle_remove = [&pkg_diff](revision& rev, const std::string& pkg_name)
        {
            bool res = false;
            if (auto rev_iter = rev.removed_pkg.find(pkg_name); rev_iter != rev.removed_pkg.end())
            {
                const auto version = rev_iter->second.version;
                auto iter = pkg_diff.installed_pkg_diff.find(pkg_name);
                if (iter != pkg_diff.installed_pkg_diff.end() && iter->second.version == version)
                {
                    pkg_diff.installed_pkg_diff.erase(iter);
                }
                else
                {
                    pkg_diff.removed_pkg_diff[pkg_name] = rev_iter->second;
                }
                rev.removed_pkg.erase(rev_iter);
                res = true;
            }
            return res;
        };

        while (!revisions.empty())
        {
            auto& revision = *(revisions.begin());
            while (!revision.removed_pkg.empty())
            {
                auto [pkg_name, pkg_info] = *(revision.removed_pkg.begin());
                pkg_diff.removed_pkg_diff[pkg_name] = pkg_info;
                revision.removed_pkg.erase(pkg_name);
                bool lastly_removed = true;  // whether last operation on package was a removal
                lastly_removed = !handle_install(revision, pkg_name);
                for (auto rev = std::next(revisions.begin()); rev != revisions.end(); ++rev)
                {
                    if (lastly_removed)
                    {
                        lastly_removed = !handle_install(*rev, pkg_name);
                    }
                    else
                    {
                        lastly_removed = handle_remove(*rev, pkg_name);
                        if (lastly_removed)
                        {
                            lastly_removed = !handle_install(*rev, pkg_name);
                        }
                    }
                }
            }
            while (!revision.installed_pkg.empty())
            {
                auto [pkg_name, pkg_info] = *(revision.installed_pkg.begin());
                pkg_diff.installed_pkg_diff[pkg_name] = pkg_info;
                revision.installed_pkg.erase(pkg_name);
                bool lastly_removed = false;
                for (auto rev = ++revisions.begin(); rev != revisions.end(); ++rev)
                {
                    if (!lastly_removed)
                    {
                        lastly_removed = handle_remove(*rev, pkg_name);
                        if (lastly_removed)
                        {
                            lastly_removed = !handle_install(*rev, pkg_name);
                        }
                    }
                }
            }
            revisions.pop_front();
        }
        return pkg_diff;
    }
}  // namespace mamba
