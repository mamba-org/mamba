// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/specs/archive.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba
{
    namespace
    {
        auto parse_legacy_dist(std::string_view dist) -> std::vector<std::string>
        {
            auto dist_str = std::string(specs::strip_archive_extension(dist));
            auto split_str = util::rsplit(dist_str, "-", 2);
            if (split_str.size() != 3)
            {
                LOG_ERROR << "dist_str " << dist_str << " did not split into a correct version info.";
                throw std::runtime_error("Invalid package filename");
            }
            return split_str;
        }
    }

    auto MatchSpec::parse_version_and_build(std::string_view s)
        -> std::tuple<std::string, std::string>
    {
        const std::size_t pos = s.find_last_of(" =");
        if (pos == s.npos || pos == 0)
        {
            std::string tmp = std::string(s);
            util::replace_all(tmp, " ", "");
            return { std::move(tmp), "" };
        }
        else
        {
            char c = s[pos];
            if (c == '=')
            {
                std::size_t pm1 = pos - 1;
                char d = s[pm1];
                if (d == '=' || d == '!' || d == '|' || d == ',' || d == '<' || d == '>' || d == '~')
                {
                    auto tmp = std::string(s);
                    util::replace_all(tmp, " ", "");
                    return { tmp, "" };
                }
            }
            // c is either ' ' or pm1 is none of the forbidden chars

            auto v = std::string(s.substr(0, pos));
            auto b = std::string(s.substr(pos + 1));
            util::replace_all(v, " ", "");
            util::replace_all(b, " ", "");
            return { std::move(v), std::move(b) };
        }
    }

    auto MatchSpec::parse(std::string_view spec) -> MatchSpec
    {
        auto spec_str = std::string(spec);
        auto out = MatchSpec();
        if (spec_str.empty())
        {
            return out;
        }

        if (std::size_t idx = spec_str.find('#'); idx != std::string::npos)
        {
            spec_str = spec_str.substr(0, idx);
        }
        spec_str = util::strip(spec_str);

        if (specs::has_archive_extension(spec_str))
        {
            out.channel = specs::ChannelSpec::parse(spec_str);
            auto [path, pkg] = util::rsplit_once(out.channel->location(), '/');
            auto dist = parse_legacy_dist(pkg);
            out.m_name = dist[0];
            out.version = dist[1];
            out.build_string = dist[2];
            out.m_filename = std::string(pkg);
            out.m_url = util::path_or_url_to_url(spec_str);
            return out;
        }

        auto extract_kv = [&spec_str](const std::string& kv_string, auto& map)
        {
            static const std::regex kv_re("([a-zA-Z0-9_-]+?)=([\"\']?)([^\'\"]*?)(\\2)(?:[\'\", ]|$)");
            std::cmatch kv_match;
            const char* text_iter = kv_string.c_str();

            while (std::regex_search(text_iter, kv_match, kv_re))
            {
                auto key = kv_match[1].str();
                auto value = kv_match[3].str();
                if (key.size() == 0 || value.size() == 0)
                {
                    throw std::runtime_error(
                        util::concat(R"(key-value mismatch in brackets ")", spec_str, '"')
                    );
                }
                text_iter += kv_match.position() + kv_match.length();
                map[key] = value;
            }
        };

        std::smatch match;

        // Step 3. strip off brackets portion
        static std::regex brackets_re(".*(?:(\\[.*\\]))");
        if (std::regex_search(spec_str, match, brackets_re))
        {
            auto brackets_str = match[1].str();
            brackets_str = brackets_str.substr(1, brackets_str.size() - 2);
            extract_kv(brackets_str, out.brackets);
            spec_str.erase(
                static_cast<std::size_t>(match.position(1)),
                static_cast<std::size_t>(match.length(1))
            );
        }

        // Step 4. strip off parens portion
        static std::regex parens_re(".*(?:(\\(.*\\)))");
        if (std::regex_search(spec_str, match, parens_re))
        {
            auto parens_str = match[1].str();
            parens_str = parens_str.substr(1, parens_str.size() - 2);
            extract_kv(parens_str, out.parens);
            if (parens_str.find("optional") != parens_str.npos)
            {
                out.optional = true;
            }
            spec_str.erase(
                static_cast<std::size_t>(match.position(1)),
                static_cast<std::size_t>(match.length(1))
            );
        }

        auto m5 = util::rsplit(spec_str, ":", 2);
        auto m5_len = m5.size();
        std::string channel_str;
        if (m5_len == 3)
        {
            out.channel = specs::ChannelSpec::parse(m5[0]);
            out.m_name_space = m5[1];
            spec_str = m5[2];
        }
        else if (m5_len == 2)
        {
            out.m_name_space = m5[0];
            spec_str = m5[1];
        }
        else if (m5_len == 1)
        {
            spec_str = m5[0];
        }
        else
        {
            throw std::runtime_error("Parsing of channel / namespace / subdir failed.");
        }

        auto get_known_platforms = []() -> std::vector<std::string>
        {
            auto plats = specs::known_platform_names();
            return { plats.begin(), plats.end() };
        };

        // support faulty conda matchspecs such as `libblas=[build=*mkl]`, which is
        // the repr of `libblas=*=*mkl`
        if (spec_str.back() == '=')
        {
            spec_str.push_back('*');
        }
        // This is #6 of the spec parsing
        static std::regex version_build_re("([^ =<>!~]+)?([><!=~ ].+)?");
        std::smatch vb_match;
        if (std::regex_match(spec_str, vb_match, version_build_re))
        {
            out.m_name = vb_match[1].str();
            out.version = util::strip(vb_match[2].str());
            if (out.m_name.size() == 0)
            {
                throw std::runtime_error("Invalid spec, no package name found: " + spec_str);
            }
        }
        else
        {
            throw std::runtime_error("Invalid spec, no package name found: " + spec_str);
        }

        // # Step 7. otherwise sort out version + build
        // spec_str = spec_str and spec_str.strip()
        if (!out.version.empty())
        {
            if (out.version.find('[') != out.version.npos)
            {
                throw std::runtime_error(util::concat(
                    R"(Invalid match spec: multiple bracket sections not allowed ")",
                    spec,
                    '"'
                ));
            }

            out.version = std::string(util::strip(out.version));
            auto [pv, pb] = parse_version_and_build(std::string(util::strip(out.version)));

            out.version = pv;
            out.build_string = pb;

            // translate version '=1.2.3' to '1.2.3*'
            // is it a simple version starting with '='? i.e. '=1.2.3'
            if (out.version.size() >= 2 && out.version[0] == '=')
            {
                auto rest = out.version.substr(1);
                if (out.version[1] == '=' && out.build_string.empty())
                {
                    out.version = out.version.substr(2);
                }
                else if (rest.find_first_of("=,|") == rest.npos)
                {
                    if (out.build_string.empty() && out.version.back() != '*')
                    {
                        out.version = util::concat(out.version, "*");
                    }
                    else
                    {
                        out.version = rest;
                    }
                }
            }
        }
        else
        {
            out.version = "";
            out.build_string = "";
        }

        // TODO think about using a hash function here, (and elsewhere), like:
        // https://hbfs.wordpress.com/2017/01/10/strings-in-c-switchcase-statements/

        for (auto& [k, v] : out.brackets)
        {
            if (k == "build_number")
            {
                out.m_build_number = v;
            }
            else if (k == "build")
            {
                out.build_string = v;
            }
            else if (k == "version")
            {
                out.version = v;
            }
            else if (k == "channel")
            {
                if (!out.channel.has_value())
                {
                    out.channel = specs::ChannelSpec::parse(v);
                }
                else
                {
                    // Subdirs might have been set with a previous subdir key
                    auto subdirs = out.channel->clear_platform_filters();
                    out.channel = specs::ChannelSpec::parse(v);
                    if (!subdirs.empty())
                    {
                        out.channel = specs::ChannelSpec(
                            out.channel->clear_location(),
                            std::move(subdirs),
                            out.channel->type()
                        );
                    }
                }
            }
            else if (k == "subdir")
            {
                if (!out.channel.has_value())
                {
                    out.channel = specs::ChannelSpec("", { v }, specs::ChannelSpec::Type::Unknown);
                }
                // Subdirs specified in the channel part have higher precedence
                else if (out.channel->platform_filters().empty())
                {
                    out.channel = specs::ChannelSpec(
                        out.channel->clear_location(),
                        { v },
                        out.channel->type()
                    );
                }
            }
            else if (k == "url")
            {
                out.m_url = v;
            }
            else if (k == "fn")
            {
                out.m_filename = v;
            }
        }
        return out;
    }

    auto MatchSpec::name() const -> const std::string&
    {
        return m_name;
    }

    auto MatchSpec::name_space() const -> const std::string&
    {
        return m_name_space;
    }

    auto MatchSpec::build_number() const -> const std::string&
    {
        return m_build_number;
    }

    auto MatchSpec::filename() const -> const std::string&
    {
        return m_filename;
    }

    auto MatchSpec::url() const -> const std::string&
    {
        return m_url;
    }

    auto MatchSpec::conda_build_form() const -> std::string
    {
        std::stringstream res;
        res << m_name;
        if (!version.empty())
        {
            res << " " << version;
            // if (!build.empty() && (build != "*"))
            if (!build_string.empty())
            {
                res << " " << build_string;
            }
        }
        return res.str();
    }

    auto MatchSpec::str() const -> std::string
    {
        std::stringstream res;
        // builder = []
        // brackets = []

        // channel_matcher = self._match_components.get('channel')
        // if channel_matcher and channel_matcher.exact_value:
        //     builder.append(text_type(channel_matcher))
        // elif channel_matcher and not channel_matcher.matches_all:
        //     brackets.append("channel=%s" % text_type(channel_matcher))

        // subdir_matcher = self._match_components.get('subdir')
        // if subdir_matcher:
        //     if channel_matcher and channel_matcher.exact_value:
        //         builder.append('/%s' % subdir_matcher)
        //     else:
        //         brackets.append("subdir=%s" % subdir_matcher)

        if (channel.has_value())
        {
            res << fmt::format("{}::", *channel);
        }
        // TODO when namespaces are implemented!
        // if (!ns.empty())
        // {
        //     res << ns;
        //     res << ":";
        // }
        res << (!m_name.empty() ? m_name : "*");
        std::vector<std::string> formatted_brackets;
        bool version_exact = false;

        auto is_complex_relation = [](const std::string& s)
        { return s.find_first_of("><$^|,") != s.npos; };

        if (!version.empty())
        {
            if (is_complex_relation(version))
            {
                formatted_brackets.push_back(util::concat("version='", version, "'"));
            }
            else if (util::starts_with(version, "!=") || util::starts_with(version, "~="))
            {
                if (!build_string.empty())
                {
                    formatted_brackets.push_back(util::concat("version='", version, "'"));
                }
                else
                {
                    res << " " << version;
                }
            }
            else if (util::ends_with(version, ".*"))
            {
                res << "=" + version.substr(0, version.size() - 2);
            }
            else if (version.back() == '*')
            {
                if (version.size() == 1)
                {
                    res << "=*";
                }
                else if (util::starts_with(version, "="))
                {
                    res << version.substr(0, version.size() - 1);
                }
                else
                {
                    res << "=" + version.substr(0, version.size() - 1);
                }
            }
            else if (util::starts_with(version, "=="))
            {
                res << version;
                version_exact = true;
            }
            else
            {
                res << "==" << version;
                version_exact = true;
            }
        }

        if (!build_string.empty())
        {
            if (is_complex_relation(build_string))
            {
                formatted_brackets.push_back(util::concat("build='", build_string, '\''));
            }
            else if (build_string.find('*') != build_string.npos)
            {
                formatted_brackets.push_back(util::concat("build=", build_string));
            }
            else if (version_exact)
            {
                res << "=" << build_string;
            }
            else
            {
                formatted_brackets.push_back(util::concat("build=", build_string));
            }
        }

        std::vector<std::string> check = {
            "build_number", "track_features", "features",       "url",
            "md5",          "license",        "license_family", "fn"
        };

        if (!m_url.empty())
        {
            // erase "fn" when we have a URL
            check.pop_back();
        }
        for (const auto& key : check)
        {
            if (brackets.find(key) != brackets.end())
            {
                if (brackets.at(key).find_first_of("= ,") != std::string::npos)
                {
                    // need quoting
                    formatted_brackets.push_back(util::concat(key, "='", brackets.at(key), "'"));
                }
                else
                {
                    formatted_brackets.push_back(util::concat(key, "=", brackets.at(key)));
                }
            }
        }
        // for key in self.FIELD_NAMES:
        //     if key not in _skip and key in self._match_components:
        //         if key == 'url' and channel_matcher:
        //             # skip url in canonical str if channel already included
        //             continue
        //         value = text_type(self._match_components[key])
        //         if any(s in value for s in ', ='):
        //             brackets.append("%s='%s'" % (key, value))
        //         else:
        //             brackets.append("%s=%s" % (key, value))

        if (formatted_brackets.size())
        {
            res << "[" << util::join(",", formatted_brackets) << "]";
        }
        return res.str();
    }

    auto MatchSpec::is_simple() const -> bool
    {
        return version.empty() && build_string.empty() && m_build_number.empty();
    }

    auto MatchSpec::is_file() const -> bool
    {
        return (!m_filename.empty()) || (!m_url.empty());
    }
}
