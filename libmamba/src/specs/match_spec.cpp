// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <sstream>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "mamba/specs/archive.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::specs
{
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

    auto MatchSpec::parse_url(std::string_view spec) -> MatchSpec
    {
        auto fail_parse = [&]()
        {
            throw std::invalid_argument(
                util::concat(R"(Fail to parse MatchSpec ditribution ")", spec, '"')
            );
        };

        auto out = MatchSpec();
        out.m_channel = ChannelSpec::parse(spec);
        auto [_, pkg] = util::rsplit_once(out.m_channel->location(), '/');
        out.m_filename = std::string(pkg);
        out.m_url = util::path_or_url_to_url(spec);

        // Build string
        auto [head, tail] = util::rsplit_once(strip_archive_extension(pkg), '-');
        out.m_build_string = BuildStringSpec(std::string(tail));
        if (!head.has_value())
        {
            fail_parse();
        }

        // Version
        std::tie(head, tail) = util::rsplit_once(head.value(), '-');
        out.m_version = VersionSpec::parse(tail);
        if (!head.has_value())
        {
            fail_parse();
        }

        // Name
        out.m_name = NameSpec(std::string(head.value()));  // There may be '-' in the name

        return out;
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

        if (has_archive_extension(spec_str))
        {
            return MatchSpec::parse_url(spec_str);
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
                out.m_optional = true;
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
            out.m_channel = ChannelSpec::parse(m5[0]);
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

        // support faulty conda matchspecs such as `libblas=[build=*mkl]`, which is
        // the repr of `libblas=*=*mkl`
        if (spec_str.back() == '=')
        {
            spec_str.push_back('*');
        }
        // This is #6 of the spec parsing
        // Look for version *and* build string and separator
        auto version_and_build = std::string();
        static std::regex version_build_re("([^ =<>!~]+)?([><!=~ ].+)?");
        std::smatch vb_match;
        if (std::regex_match(spec_str, vb_match, version_build_re))
        {
            out.m_name = NameSpec(vb_match[1].str());
            version_and_build = util::strip(vb_match[2].str());
        }
        else
        {
            throw std::runtime_error("Invalid spec, no package name found: " + spec_str);
        }

        // # Step 7. otherwise sort out version + build
        // spec_str = spec_str and spec_str.strip()
        if (!version_and_build.empty())
        {
            if (version_and_build.find('[') != std::string::npos)
            {
                throw std::runtime_error(util::concat(
                    R"(Invalid match spec: multiple bracket sections not allowed ")",
                    spec,
                    '"'
                ));
            }

            auto [pv, pb] = parse_version_and_build(version_and_build);

            out.m_version = VersionSpec::parse(pv);
            out.m_build_string = BuildStringSpec(std::string(pb));
        }
        else  // no-op
        {
            out.m_version = {};
            out.m_build_string = {};
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
                out.m_build_string = MatchSpec::BuildStringSpec(std::string(v));
            }
            else if (k == "version")
            {
                out.m_version = VersionSpec::parse(v);
            }
            else if (k == "channel")
            {
                if (!out.m_channel.has_value())
                {
                    out.m_channel = ChannelSpec::parse(v);
                }
                else
                {
                    // Subdirs might have been set with a previous subdir key
                    auto subdirs = out.m_channel->clear_platform_filters();
                    out.m_channel = ChannelSpec::parse(v);
                    if (!subdirs.empty())
                    {
                        out.m_channel = ChannelSpec(
                            out.m_channel->clear_location(),
                            std::move(subdirs),
                            out.m_channel->type()
                        );
                    }
                }
            }
            else if (k == "subdir")
            {
                if (!out.m_channel.has_value())
                {
                    out.m_channel = ChannelSpec("", { v }, ChannelSpec::Type::Unknown);
                }
                // Subdirs specified in the channel part have higher precedence
                else if (out.m_channel->platform_filters().empty())
                {
                    out.m_channel = ChannelSpec(
                        out.m_channel->clear_location(),
                        { v },
                        out.m_channel->type()
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

    auto MatchSpec::channel() const -> const std::optional<ChannelSpec>&
    {
        return m_channel;
    }

    void MatchSpec::set_channel(std::optional<ChannelSpec> chan)
    {
        m_channel = std::move(chan);
    }

    auto MatchSpec::name_space() const -> const std::string&
    {
        return m_name_space;
    }

    void MatchSpec::set_name_space(std::string ns)
    {
        m_name_space = std::move(ns);
    }

    auto MatchSpec::name() const -> const NameSpec&
    {
        return m_name;
    }

    void MatchSpec::set_name(NameSpec name)
    {
        m_name = std::move(name);
    }

    auto MatchSpec::version() const -> const VersionSpec&
    {
        return m_version;
    }

    void MatchSpec::set_version(VersionSpec ver)
    {
        m_version = std::move(ver);
    }

    auto MatchSpec::build_number() const -> const std::string&
    {
        return m_build_number;
    }

    void MatchSpec::set_build_number(std::string bn)
    {
        m_build_number = std::move(bn);
    }

    auto MatchSpec::build_string() const -> const BuildStringSpec&
    {
        return m_build_string;
    }

    auto MatchSpec::optional() const -> bool
    {
        return m_optional;
    }

    void MatchSpec::set_optional(bool opt)
    {
        m_optional = opt;
    }

    void MatchSpec::set_build_string(BuildStringSpec bs)
    {
        m_build_string = std::move(bs);
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
        const bool has_version = !m_version.is_explicitly_free();
        const bool has_build_str = !m_build_string.is_free();
        if (has_version)
        {
            if (has_build_str)
            {
                return fmt::format("{} {:b} {}", m_name, m_version, m_build_string);
            }
            else
            {
                return fmt::format("{} {:b}", m_name, m_version);
            }
        }
        if (has_build_str)
        {
            return fmt::format("{} * {}", m_name, m_build_string);
        }
        return fmt::format("{}", m_name);
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

        if (m_channel.has_value())
        {
            res << fmt::format("{}::", *m_channel);
        }
        // TODO when namespaces are implemented!
        // if (!ns.empty())
        // {
        //     res << ns;
        //     res << ":";
        // }
        res << m_name.str();
        std::vector<std::string> formatted_brackets;

        auto is_complex_relation = [](const std::string& s)
        { return s.find_first_of("><$^|,") != s.npos; };

        if (!m_version.is_explicitly_free())
        {
            auto ver = m_version.str();
            if (is_complex_relation(ver))  // TODO do on VersionSpec
            {
                formatted_brackets.push_back(util::concat("version='", ver, "'"));
            }
            else
            {
                res << ver;
                // version_exact = true;
            }
        }

        if (!m_build_string.is_free())
        {
            if (m_build_string.is_exact())
            {
                res << "=" << m_build_string.str();
            }
            else
            {
                formatted_brackets.push_back(util::concat("build='", m_build_string.str(), '\''));
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
        return m_version.is_explicitly_free() && m_build_string.is_free() && m_build_number.empty();
    }

    auto MatchSpec::is_file() const -> bool
    {
        return (!m_filename.empty()) || (!m_url.empty());
    }
}
