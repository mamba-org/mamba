// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/environment.hpp"

namespace mamba
{
    std::vector<std::string> parse_legacy_dist(std::string dist_str)
    {
        dist_str = strip_package_extension(dist_str).string();
        auto split_str = rsplit(dist_str, "-", 2);
        if (split_str.size() != 3)
        {
            LOG_ERROR << "dist_str " << dist_str << " did not split into a correct version info.";
            throw std::runtime_error("Invalid package filename");
        }
        return split_str;
    }


    MatchSpec::MatchSpec(const std::string& i_spec)
        : spec(i_spec)
    {
        parse();
    }

    std::tuple<std::string, std::string> MatchSpec::parse_version_and_build(const std::string& s)
    {
        std::size_t pos = s.find_last_of(" =");
        if (pos == s.npos || pos == 0)
        {
            std::string tmp = s;
            replace_all(tmp, " ", "");
            return { tmp, "" };
        }
        else
        {
            char c = s[pos];
            if (c == '=')
            {
                std::size_t pm1 = pos - 1;
                char d = s[pm1];
                if (d == '=' || d == '!' || d == '|' || d == ',' || d == '<' || d == '>'
                    || d == '~')
                {
                    std::string tmp = s;
                    replace_all(tmp, " ", "");
                    return { tmp, "" };
                }
            }
            // c is either ' ' or pm1 is none of the forbidden chars

            std::string v = s.substr(0, pos), b = s.substr(pos + 1);
            replace_all(v, " ", "");
            replace_all(b, " ", "");
            return { v, b };
        }
    }

    void MatchSpec::parse()
    {
        LOG_INFO << "Parsing MatchSpec " << spec;
        std::string spec_str = spec;

        std::size_t idx = spec_str.find('#');
        if (idx != std::string::npos)
        {
            spec_str = spec_str.substr(0, idx);
        }
        spec_str = strip(spec_str);

        if (is_package_file(spec_str))
        {
            if (!has_scheme(spec_str))
            {
                LOG_INFO << "need to expand path!";
                spec_str = path_to_url(fs::absolute(env::expand_user(spec_str)).string());
            }
            auto& parsed_channel = make_channel(spec_str);

            if (parsed_channel.package_filename())
            {
                auto dist = parse_legacy_dist(*parsed_channel.package_filename());

                name = dist[0];
                version = dist[1];
                build = dist[2];

                channel = parsed_channel.canonical_name();
                // TODO how to handle this with multiple platforms?
                subdir = parsed_channel.platforms()[0];
                fn = *parsed_channel.package_filename();
                url = spec_str;
                is_file = true;
            }
            return;
        }

        auto extract_kv = [&spec_str](const std::string& kv_string, auto& map)
        {
            static std::regex kv_re("([a-zA-Z0-9_-]+?)=([\"\']?)([^\'\"]*?)(\\2)(?:[\'\", ]|$)");
            std::cmatch kv_match;
            const char* text_iter = kv_string.c_str();

            while (std::regex_search(text_iter, kv_match, kv_re))
            {
                auto key = kv_match[1].str();
                auto value = kv_match[3].str();
                if (key.size() == 0 || value.size() == 0)
                {
                    throw std::runtime_error("key-value mismatch in brackets " + spec_str);
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
            extract_kv(brackets_str, brackets);
            spec_str.erase(match.position(1), match.length(1));
        }

        // Step 4. strip off parens portion
        static std::regex parens_re(".*(?:(\\(.*\\)))");
        if (std::regex_search(spec_str, match, parens_re))
        {
            auto parens_str = match[1].str();
            parens_str = parens_str.substr(1, parens_str.size() - 2);
            extract_kv(parens_str, this->parens);
            if (parens_str.find("optional") != parens_str.npos)
            {
                optional = true;
            }
            spec_str.erase(match.position(1), match.length(1));
        }

        auto m5 = rsplit(spec_str, ":", 2);
        auto m5_len = m5.size();
        std::string channel_str;
        if (m5_len == 3)
        {
            channel = m5[0];
            ns = m5[1];
            spec_str = m5[2];
        }
        else if (m5_len == 2)
        {
            ns = m5[0];
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
        // TODO implement Channel, and parsing of the channel here!
        // channel = subdir = channel_str;
        // channel, subdir = _parse_channel(channel_str)
        // if 'channel' in brackets:
        //     b_channel, b_subdir = _parse_channel(brackets.pop('channel'))
        //     if b_channel:
        //         channel = b_channel
        //     if b_subdir:
        //         subdir = b_subdir
        // if 'subdir' in brackets:
        //     subdir = brackets.pop('subdir')

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
            name = vb_match[1].str();
            version = strip(vb_match[2].str());
            if (name.size() == 0)
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
        if (!version.empty())
        {
            if (version.find("[") != version.npos)
            {
                throw std::runtime_error(
                    "Invalid match spec: multiple bracket sections not allowed " + spec);
            }

            version = std::string(strip(version));
            auto [pv, pb] = parse_version_and_build(std::string(strip(version)));

            version = pv;
            build = pb;

            // translate version '=1.2.3' to '1.2.3*'
            // is it a simple version starting with '='? i.e. '=1.2.3'
            if (version.size() >= 2 && version[0] == '=')
            {
                auto rest = version.substr(1);
                if (version[1] == '=' && build.empty())
                {
                    version = version.substr(2);
                }
                else if (rest.find_first_of("=,|") == rest.npos)
                {
                    if (build.empty() && version.back() != '*')
                    {
                        version = concat(version, "*");
                    }
                    else
                    {
                        version = rest;
                    }
                }
            }
        }
        else
        {
            version = "";
            build = "";
        }

        // TODO think about using a hash function here, (and elsewhere), like:
        // https://hbfs.wordpress.com/2017/01/10/strings-in-c-switchcase-statements/
        for (auto& [k, v] : brackets)
        {
            if (k == "build_number")
            {
                build_number = v;
            }
            else if (k == "build")
            {
                build = v;
            }
            else if (k == "version")
            {
                version = v;
            }
            else if (k == "channel")
            {
                channel = v;
            }
            else if (k == "subdir")
            {
                subdir = v;
            }
            else if (k == "url")
            {
                is_file = true;
                url = v;
            }
            else if (k == "fn")
            {
                is_file = true;
                fn = v;
            }
        }
    }

    std::string MatchSpec::conda_build_form() const
    {
        assert(!name.empty());
        std::stringstream res;
        res << name;
        if (!version.empty())
        {
            res << " " << version;
            // if (!build.empty() && (build != "*"))
            if (!build.empty())
            {
                res << " " << build;
            }
        }
        return res.str();
    }

    std::string MatchSpec::str() const
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

        if (!channel.empty())
        {
            res << channel;
            if (!subdir.empty())
            {
                res << "/" << subdir;
            }
            res << "::";
        }
        // TODO when namespaces are implemented!
        // if (!ns.empty())
        // {
        //     res << ns;
        //     res << ":";
        // }
        res << (!name.empty() ? name : "*");
        std::vector<std::string> formatted_brackets;
        bool version_exact = false;

        auto is_complex_relation
            = [](const std::string& s) { return s.find_first_of("><$^|,") != s.npos; };

        if (!version.empty())
        {
            if (is_complex_relation(version))
            {
                formatted_brackets.push_back(concat("version='", version, "'"));
            }
            else if (starts_with(version, "!=") || starts_with(version, "~="))
            {
                if (!build.empty())
                {
                    formatted_brackets.push_back(concat("version='", version, "'"));
                }
                else
                {
                    res << " " << version;
                }
            }
            else if (ends_with(version, ".*"))
            {
                res << "=" + version.substr(0, version.size() - 2);
            }
            else if (version.back() == '*')
            {
                if (version.size() == 1)
                {
                    res << "=*";
                }
                else if (starts_with(version, "="))
                {
                    res << version.substr(0, version.size() - 1);
                }
                else
                {
                    res << "=" + version.substr(0, version.size() - 1);
                }
            }
            else if (starts_with(version, "=="))
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

        if (!build.empty())
        {
            if (is_complex_relation(build))
            {
                formatted_brackets.push_back(concat("build='", build, "'"));
            }
            else if (build.find("*") != build.npos)
            {
                formatted_brackets.push_back(concat("build=", build));
            }
            else if (version_exact)
            {
                res << "=" << build;
            }
            else
            {
                formatted_brackets.push_back(concat("build=", build));
            }
        }

        std::vector<std::string> check
            = { "build_number", "track_features", "features",       "url",
                "md5",          "license",        "license_family", "fn" };

        if (!url.empty())
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
                    formatted_brackets.push_back(concat(key, "='", brackets.at(key), "'"));
                }
                else
                {
                    formatted_brackets.push_back(concat(key, "=", brackets.at(key)));
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
            res << "[" << join(",", formatted_brackets) << "]";
        }
        return res.str();
    }

    bool MatchSpec::is_simple() const
    {
        return version.empty() && build.empty() && build_number.empty();
    }
}  // namespace mamba
