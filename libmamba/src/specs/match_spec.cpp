// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

#include <fmt/format.h>

#include "mamba/specs/archive.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/parsers.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    auto MatchSpec::parse_url(std::string_view spec) -> MatchSpec
    {
        auto fail_parse = [&]()
        {
            throw std::invalid_argument(
                util::concat(R"(Fail to parse MatchSpec ditribution ")", spec, '"')
            );
        };

        auto out = MatchSpec();
        // Channel is also read for the filename so no need to set it.
        out.m_channel = UnresolvedChannel::parse(spec)
                            .or_else([](specs::ParseError&& error) { throw std::move(error); })
                            .value();
        auto [_, pkg] = util::rsplit_once(out.m_channel->location(), '/');

        // Build string
        auto [head, tail] = util::rsplit_once(strip_archive_extension(pkg), '-');
        out.m_build_string = BuildStringSpec(std::string(tail));
        if (!head.has_value())
        {
            fail_parse();
        }

        // Version
        std::tie(head, tail) = util::rsplit_once(head.value(), '-');
        out.m_version = VersionSpec::parse(tail)
                            .or_else([](ParseError&& error) { throw std::move(error); })
                            .value();
        if (!head.has_value())
        {
            fail_parse();
        }

        // Name
        out.m_name = NameSpec(std::string(head.value()));  // There may be '-' in the name

        return out;
    }

    namespace
    {
        auto parse_version_and_build(std::string_view s)
            -> std::pair<std::string_view, std::string_view>
        {
            const std::size_t pos = s.find_last_of(" =");
            if (pos == s.npos || pos == 0)
            {
                return { s, {} };
            }

            if (char c = s[pos]; c == '=')
            {
                std::size_t pm1 = pos - 1;
                char d = s[pm1];
                if (d == '=' || d == '!' || d == '|' || d == ',' || d == '<' || d == '>' || d == '~')
                {
                    return { s, {} };
                }
            }
            // c is either ' ' or pm1 is none of the forbidden chars

            return { s.substr(0, pos), s.substr(pos + 1) };
        }
    }

    namespace
    {
        inline constexpr auto open_or_quote = std::array{
            MatchSpec::prefered_list_open,
            MatchSpec::alt_list_open,
            MatchSpec::prefered_quote,
            MatchSpec::alt_quote,
        };

        inline constexpr auto close_or_quote = std::array{
            MatchSpec::prefered_list_close,
            MatchSpec::alt_list_close,
            MatchSpec::prefered_quote,
            MatchSpec::alt_quote,
        };

        /** Return true if the string is a valid hash hex representation. */
        auto is_hash(std::string_view text) -> bool
        {
            constexpr auto is_hash_char = [](char c) -> bool
            {
                auto const lower = util::to_lower(c);
                return util::is_digit(c) || (lower == 'a') || (lower == 'b') || (lower == 'c')
                       || (lower == 'd') || (lower == 'e') || (lower == 'f');
            };
            return std::all_of(text.cbegin(), text.cend(), is_hash_char);
        }

        auto rfind_channel_namespace_split(std::string_view str)
        {
            return util::rfind_not_in_parentheses(
                       str,
                       MatchSpec::channel_namespace_spec_sep,
                       open_or_quote,
                       close_or_quote
            )
                // FIXME temporary while MatchSpec::parse does not return ``exepted``.
                .or_else(
                    [&](const auto&) {
                        throw std::invalid_argument(
                            fmt::format(R"(Invalid parenthesis in MatchSpec "{}")", str)
                        );
                    }
                )
                .value();
            ;
        }

        auto split_channel_namespace_spec(std::string_view str)
            -> std::tuple<std::string_view, std::string_view, std::string_view>
        {
            const auto spec_pos = rfind_channel_namespace_split(str);
            if (spec_pos != std::string_view::npos)
            {
                const auto spec = str.substr(spec_pos + 1);
                const auto ns_pos = rfind_channel_namespace_split(str.substr(0, spec_pos));
                if (ns_pos != std::string_view::npos)
                {
                    return {
                        /* channel= */ str.substr(0, ns_pos),
                        /* namespace= */ str.substr(ns_pos + 1, spec_pos),
                        /* spec= */ str.substr(spec_pos + 1),
                    };
                }
                else
                {
                    return {
                        /* channel= */ "",
                        /* namespace= */ spec.substr(0, spec_pos),
                        /* spec= */ str.substr(spec_pos + 1),
                    };
                }
            }
            return {
                /* channel= */ "",
                /* namespace= */ "",
                /* spec= */ str,
            };
        }

        auto find_attribute_split(std::string_view str)
        {
            return util::find_not_in_parentheses(str, MatchSpec::attribute_sep, open_or_quote, close_or_quote)
                // FIXME temporary while MatchSpec::parse does not return ``exepted``.
                .or_else(
                    [&](const auto&) {
                        throw std::invalid_argument(
                            fmt::format(R"(Invalid parenthesis in MatchSpec "{}")", str)
                        );
                    }
                )
                .value();
            ;
        }

        auto strip_whitespace_quotes(std::string_view str) -> std::string_view
        {
            return util::strip_if(
                str,
                [](char c) -> bool {
                    return !util::is_graphic(c) || (c == MatchSpec::prefered_quote)
                           || (c == MatchSpec::alt_quote);
                }

            );
        }

        auto is_true_string(std::string_view str) -> bool
        {
            return util::starts_with_any(str, std::array{ 'y', 'Y', 't', 'T', '1' });
        }

        void
        set_single_matchspec_attribute(MatchSpec& spec, std::string_view attr, std::string_view val)
        {
            if (attr == "build_number")
            {
                spec.set_build_number(BuildNumberSpec::parse(val)
                                          .or_else([](ParseError&& error) { throw std::move(error); })
                                          .value()

                );
            }
            else if ((attr == "build") || (attr == "build_strig"))
            {
                spec.set_build_string(MatchSpec::BuildStringSpec(std::string(val)));
            }
            else if (attr == "version")
            {
                spec.set_version(VersionSpec::parse(val)
                                     .or_else([](ParseError&& error) { throw std::move(error); })
                                     .value());
            }
            else if ((attr == "channel") || (attr == "url"))
            {
                spec.set_channel(UnresolvedChannel::parse(val)
                                     .or_else([](ParseError&& error) { throw std::move(error); })
                                     .value());
            }
            else if (attr == "subdir")
            {
                if (auto chan = spec.channel(); !chan.has_value() || chan->platform_filters().empty())
                {
                    spec.set_subdirs({ UnresolvedChannel::parse_platform_list(val) });
                }
            }
            else if ((attr == "fn") || (attr == "filename"))
            {
                spec.set_filename(std::string(val));
            }
            else if (attr == "md5")
            {
                spec.set_md5(std::string(val));
            }
            else if (attr == "sha256")
            {
                spec.set_sha256(std::string(val));
            }
            else if (attr == "license")
            {
                spec.set_license(std::string(val));
            }
            else if (attr == "license_family")
            {
                spec.set_license_family(std::string(val));
            }
            else if (attr == "features")
            {
                spec.set_features(std::string(val));
            }
            else if (attr == "track_features")
            {
                spec.set_track_features(std::string(val));
            }
            else if (attr == "optional")
            {
                spec.set_optional(is_true_string(val));
            }
        }

        void set_matchspec_attributes(MatchSpec& spec, std::string_view attrs)
        {
            const auto next_pos = find_attribute_split(attrs);

            auto [key, value] = util::split_once(attrs.substr(0, next_pos), MatchSpec::attribute_assign);
            set_single_matchspec_attribute(
                spec,
                util::to_lower(util::strip(key)),
                strip_whitespace_quotes(value.value_or("true"))
            );

            if (next_pos != std::string_view::npos)
            {
                set_matchspec_attributes(spec, attrs.substr(next_pos + 1));
            }
        };
    }

    auto MatchSpec::parse(std::string_view spec) -> MatchSpec
    {
        static constexpr auto npos = std::string_view::npos;
        spec = util::strip(spec);
        if (spec.empty())
        {
            return {};
        }

        // A plain URL like https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda
        if (has_archive_extension(spec))
        {
            return MatchSpec::parse_url(spec);
        }

        // A URL with hash, generated by `mamba env export --explicit` like
        // https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda#7dbaa197d7ba6032caf7ae7f32c1efa0
        if (const auto idx = spec.rfind(url_md5_sep); idx != npos)
        {
            auto url = spec.substr(0, idx);
            auto hash = spec.substr(idx + 1);
            if (has_archive_extension(url))
            {
                auto out = MatchSpec::parse_url(url);
                if (is_hash(hash))
                {
                    out.set_md5(std::string(hash));
                }
                return out;
            }
        }

        auto out = MatchSpec();

        // Split full matchspec like
        // ``https://channel[plat]:namespace:spec >=3 [attr="val", ...]``
        // into:
        //   - ``https://channel[plat]``
        //   - ``namespace``
        //   - ``spec >=3 [attr="val", ...]``
        auto chan = std::string_view();
        std::tie(chan, out.m_name_space, spec) = split_channel_namespace_spec(spec);
        if (!chan.empty())
        {
            out.m_channel = UnresolvedChannel::parse(chan)
                                .or_else([](specs::ParseError&& error) { throw std::move(error); })
                                .value();
        }

        auto spec_str = std::string(spec);

        std::smatch match;

        // Step 3. strip off brackets portion
        static std::regex brackets_re(".*(?:(\\[.*\\]))");
        if (std::regex_search(spec_str, match, brackets_re))
        {
            auto brackets_str = match[1].str();
            brackets_str = brackets_str.substr(1, brackets_str.size() - 2);
            set_matchspec_attributes(out, brackets_str);
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
            set_matchspec_attributes(out, parens_str);
            if (parens_str.find("optional") != parens_str.npos)
            {
                out.extra().optional = true;
            }
            spec_str.erase(
                static_cast<std::size_t>(match.position(1)),
                static_cast<std::size_t>(match.length(1))
            );
        }

        // support faulty conda matchspecs such as `libblas=[build=*mkl]`, which is
        // the repr of `libblas=*=*mkl`
        if (spec_str.back() == '=')
        {
            spec_str.erase(spec_str.end() - 1);
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

            auto [version_str, build_string_str] = parse_version_and_build(version_and_build);
            if (!version_str.empty())
            {
                out.m_version = VersionSpec::parse(version_str)
                                    .or_else([](ParseError&& error) { throw std::move(error); })
                                    .value();
            }
            if (!build_string_str.empty())
            {
                out.m_build_string = MatchSpec::BuildStringSpec(std::string(build_string_str));
            }
        }

        return out;
    }

    auto MatchSpec::channel_is_file() const -> bool
    {
        if (const auto& chan = channel(); chan.has_value())
        {
            auto type = chan->type();
            using Type = typename UnresolvedChannel::Type;
            return (type == Type::PackageURL) || (type == Type::PackagePath);
        }
        return false;
    }

    auto MatchSpec::channel_filename() const -> std::string_view
    {
        if (channel_is_file())
        {
            assert(channel().has_value());
            auto [_, pkg] = util::rsplit_once(channel()->location(), '/');
            return pkg;
        }
        return {};
    }

    void MatchSpec::set_channel_filename(std::string val)
    {
        assert(channel().has_value());
        assert(channel_is_file());
        auto location = m_channel->clear_location();
        auto [base, pkg] = util::rsplit_once(location, '/');
        assert(base.has_value());
        location = base.value_or("");
        location += val;
        set_channel({ UnresolvedChannel(
            std::move(location),
            m_channel->clear_platform_filters(),
            m_channel->type()
        ) });
    }

    auto MatchSpec::channel() const -> const std::optional<UnresolvedChannel>&
    {
        return m_channel;
    }

    void MatchSpec::set_channel(std::optional<UnresolvedChannel> chan)
    {
        m_channel = std::move(chan);
        // Channel filename take precedence
        if (channel_is_file() && !extra_filename().empty())
        {
            set_extra_filename({});
        }
    }

    auto MatchSpec::extra_filename() const -> std::string_view
    {
        if (m_extra.has_value())
        {
            return m_extra->filename;
        }
        return {};
    }

    void MatchSpec::set_extra_filename(std::string val)
    {
        if (val != extra_filename())  // Avoid allocating extra to set the default value
        {
            extra().filename = std::move(val);
        }
    }

    auto MatchSpec::filename() const -> std::string_view
    {
        if (channel_is_file())
        {
            return channel_filename();
        }
        return extra_filename();
    }

    void MatchSpec::set_filename(std::string val)
    {
        if (channel_is_file())
        {
            set_channel_filename(std::move(val));
            set_extra_filename("");
        }
        else
        {
            set_extra_filename(std::move(val));
        }
    }

    auto MatchSpec::is_file() const -> bool
    {
        return !filename().empty();
    }

    auto MatchSpec::extra_subdirs() const -> std::optional<subdir_list_const_ref>
    {
        if (m_extra.has_value() && !m_extra->subdirs.empty())
        {
            return { std::cref(m_extra->subdirs) };
        }
        return {};
    }

    void MatchSpec::set_extra_subdirs(subdir_list val)
    {
        // Avoid allocating extra to set the default value
        if (m_extra.has_value() || !val.empty())
        {
            extra().subdirs = std::move(val);
        }
    }

    auto MatchSpec::subdirs() const -> std::optional<subdir_list_const_ref>
    {
        if (m_channel.has_value() && !m_channel->platform_filters().empty())
        {
            return { std::cref(m_channel->platform_filters()) };
        }
        return extra_subdirs();
    }

    void MatchSpec::set_subdirs(subdir_list val)
    {
        if (m_channel.has_value())
        {
            m_channel = UnresolvedChannel(
                m_channel->clear_location(),
                std::move(val),
                m_channel->type()
            );
            set_extra_subdirs({});
        }
        else
        {
            extra().subdirs = std::move(val);
        }
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

    auto MatchSpec::build_number() const -> const BuildNumberSpec&
    {
        return m_build_number;
    }

    void MatchSpec::set_build_number(BuildNumberSpec bn)
    {
        m_build_number = std::move(bn);
    }

    auto MatchSpec::build_string() const -> const BuildStringSpec&
    {
        return m_build_string;
    }

    void MatchSpec::set_build_string(BuildStringSpec bs)
    {
        m_build_string = std::move(bs);
    }

    auto MatchSpec::md5() const -> std::string_view
    {
        if (m_extra.has_value())
        {
            return m_extra->md5;
        }
        return "";
    }

    void MatchSpec::set_md5(std::string val)
    {
        if (val != md5())  // Avoid allocating extra to set the default value
        {
            extra().md5 = std::move(val);
        }
    }

    auto MatchSpec::sha256() const -> std::string_view
    {
        if (m_extra.has_value())
        {
            return m_extra->sha256;
        }
        return "";
    }

    void MatchSpec::set_sha256(std::string val)
    {
        if (val != sha256())  // Avoid allocating extra to set the default value
        {
            extra().sha256 = std::move(val);
        }
    }

    auto MatchSpec::license() const -> std::string_view
    {
        if (m_extra.has_value())
        {
            return m_extra->license;
        }
        return "";
    }

    void MatchSpec::set_license(std::string val)
    {
        if (val != license())  // Avoid allocating extra to set the default value
        {
            extra().license = std::move(val);
        }
    }

    auto MatchSpec::license_family() const -> std::string_view
    {
        if (m_extra.has_value())
        {
            return m_extra->license_family;
        }
        return "";
    }

    void MatchSpec::set_license_family(std::string val)
    {
        if (val != license_family())  // Avoid allocating extra to set the default value
        {
            extra().license_family = std::move(val);
        }
    }

    auto MatchSpec::features() const -> std::string_view
    {
        if (m_extra.has_value())
        {
            return m_extra->features;
        }
        return "";
    }

    void MatchSpec::set_features(std::string val)
    {
        if (val != features())  // Avoid allocating extra to set the default value
        {
            extra().features = std::move(val);
        }
    }

    auto MatchSpec::track_features() const -> std::string_view
    {
        if (m_extra.has_value())
        {
            return m_extra->track_features;
        }
        return "";
    }

    void MatchSpec::set_track_features(std::string val)
    {
        if (val != track_features())  // Avoid allocating extra to set the default value
        {
            extra().track_features = std::move(val);
        }
    }

    auto MatchSpec::optional() const -> bool
    {
        return m_extra.has_value() && m_extra->optional;
    }

    void MatchSpec::set_optional(bool opt)
    {
        if (opt != optional())  // Avoid allocating extra to set the default value
        {
            extra().optional = opt;
        }
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

        // TODO change as attribute if complex URL, and has "url" if PackageUrl
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

        auto maybe_quote = [](std::string_view data) -> std::string_view
        {
            if (auto pos = data.find_first_of(R"( =")"); pos != std::string_view::npos)
            {
                if (util::contains(data.substr(pos), '"'))
                {
                    return "'";
                }
                return R"(")";
            }
            return "";
        };

        if (const auto& num = build_number(); !num.is_explicitly_free())
        {
            formatted_brackets.push_back(util::concat("build_number=", num.str()));
        }
        if (const auto& tf = track_features(); !tf.empty())
        {
            const auto& q = maybe_quote(tf);
            formatted_brackets.push_back(util::concat("track_features=", q, tf, q));
        }
        if (const auto& feats = features(); !feats.empty())
        {
            const auto& q = maybe_quote(feats);
            formatted_brackets.push_back(util::concat("features=", q, feats, q));
        }
        else if (const auto& fn = filename(); !fn.empty() && !channel_is_file())
        {
            // No "fn" when we have a URL
            const auto& q = maybe_quote(fn);
            formatted_brackets.push_back(util::concat("fn=", q, fn, q));
        }
        if (const auto& hash = md5(); !hash.empty())
        {
            formatted_brackets.push_back(util::concat("md5=", hash));
        }
        if (const auto& hash = sha256(); !hash.empty())
        {
            formatted_brackets.push_back(util::concat("sha256=", hash));
        }
        if (const auto& l = license(); !l.empty())
        {
            formatted_brackets.push_back(util::concat("license=", l));
        }
        if (const auto& lf = license_family(); !lf.empty())
        {
            formatted_brackets.push_back(util::concat("license_family=", lf));
        }
        if (optional())
        {
            formatted_brackets.emplace_back("optional");
        }

        if (!formatted_brackets.empty())
        {
            res << "[" << util::join(",", formatted_brackets) << "]";
        }
        return res.str();
    }

    auto MatchSpec::is_simple() const -> bool
    {
        return m_version.is_explicitly_free() && m_build_string.is_free()
               && m_build_number.is_explicitly_free();
    }

    auto MatchSpec::extra() -> ExtraMembers&
    {
        if (!m_extra.has_value())
        {
            m_extra.emplace();
        }
        return *m_extra;
    }

    namespace match_spec_literals
    {
        auto operator""_ms(const char* str, std::size_t len) -> MatchSpec
        {
            return MatchSpec::parse({ str, len });
        }
    }
}
