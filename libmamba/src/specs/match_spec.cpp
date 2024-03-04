// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

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
    auto MatchSpec::parse_url(std::string_view spec) -> expected_parse_t<MatchSpec>
    {
        auto out = MatchSpec();

        // Channel is also read for the filename so no need to set it.
        auto maybe_channel = UnresolvedChannel::parse(spec);
        if (maybe_channel.has_value())
        {
            out.m_channel = std::move(maybe_channel).value();
        }
        else
        {
            return make_unexpected_parse(std::move(maybe_channel).error());
        }

        auto [_, pkg] = util::rsplit_once(out.m_channel->location(), '/');

        // Build string
        auto [head, tail] = util::rsplit_once(strip_archive_extension(pkg), '-');
        out.m_build_string = BuildStringSpec(std::string(tail));

        if (!head.has_value())
        {
            return make_unexpected_parse(
                fmt::format(R"(Missing name and version in filename "{}".)", pkg)
            );
        }

        // Version
        std::tie(head, tail) = util::rsplit_once(head.value(), '-');
        auto maybe_version = VersionSpec::parse(tail);
        if (maybe_version.has_value())
        {
            out.m_version = std::move(maybe_version).value();
        }
        else
        {
            return make_unexpected_parse(std::move(maybe_channel).error());
        }

        if (!head.has_value())
        {
            return make_unexpected_parse(fmt::format(R"(Missing name in filename "{}".)", pkg));
        }

        // Name
        out.m_name = NameSpec(std::string(head.value()));  // There may be '-' in the name

        return { std::move(out) };
    }

    namespace
    {
        inline constexpr auto open_or_quote_tokens = std::array{
            MatchSpec::prefered_list_open,
            MatchSpec::alt_list_open,
            MatchSpec::prefered_quote,
            MatchSpec::alt_quote,
        };

        inline constexpr auto close_or_quote_tokens = std::array{
            MatchSpec::prefered_list_close,
            MatchSpec::alt_list_close,
            MatchSpec::prefered_quote,
            MatchSpec::alt_quote,
        };

        template <typename Range, typename T>
        [[nodiscard]] constexpr auto contains(const Range& range, T elem) -> bool
        {
            return std::find(range.cbegin(), range.cend(), elem) != range.cend();
        }

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

        auto rfind_channel_namespace_split(std::string_view str) -> expected_parse_t<std::size_t>
        {
            return util::rfind_not_in_parentheses(
                       str,
                       MatchSpec::channel_namespace_spec_sep,
                       open_or_quote_tokens,
                       close_or_quote_tokens
            )
                .map_error(
                    [&](const auto&)
                    { return ParseError(fmt::format(R"(Parentheses mismatch in "{}".)", str)); }
                );
        }

        using ChanNsSpecSplit = std::tuple<std::string_view, std::string_view, std::string_view>;

        auto split_channel_namespace_spec(std::string_view str) -> expected_parse_t<ChanNsSpecSplit>
        {
            return rfind_channel_namespace_split(str).and_then(
                [&](std::size_t spec_pos) -> expected_parse_t<ChanNsSpecSplit>
                {
                    if (spec_pos == std::string_view::npos)
                    {
                        return { { /* channel= */ "", /* namespace= */ "", /* spec= */ str } };
                    }

                    return rfind_channel_namespace_split(str.substr(0, spec_pos))
                        .transform(
                            [&](std::size_t ns_pos) -> ChanNsSpecSplit
                            {
                                if (ns_pos == std::string_view::npos)
                                {
                                    return {
                                        /* channel= */ "",
                                        /* namespace= */ str.substr(0, spec_pos),
                                        /* spec= */ str.substr(spec_pos + 1),
                                    };
                                }
                                return {
                                    /* channel= */ str.substr(0, ns_pos),
                                    /* namespace= */ str.substr(ns_pos + 1, spec_pos),
                                    /* spec= */ str.substr(spec_pos + 1),
                                };
                            }
                        );
                }
            );
        }

        auto find_attribute_split(std::string_view str) -> expected_parse_t<std::size_t>
        {
            return util::find_not_in_parentheses(
                       str,
                       MatchSpec::attribute_sep,
                       open_or_quote_tokens,
                       close_or_quote_tokens
            )
                .map_error(
                    [&](const auto&)
                    { return ParseError(fmt::format(R"(Parentheses mismatch in "{}".)", str)); }
                );
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

        [[nodiscard]] auto set_single_matchspec_attribute_impl(  //
            MatchSpec& spec,
            std::string_view attr,
            std::string_view val
        ) -> expected_parse_t<void>
        {
            if (attr == "build_number")
            {
                return BuildNumberSpec::parse(val).transform(
                    [&](BuildNumberSpec&& bn) { spec.set_build_number(std::move(bn)); }
                );
            }
            if ((attr == "build") || (attr == "build_string"))
            {
                spec.set_build_string(MatchSpec::BuildStringSpec(std::string(val)));
                return {};
            }
            if (attr == "version")
            {
                return VersionSpec::parse(val).transform([&](VersionSpec&& vs)
                                                         { spec.set_version(std::move(vs)); });
            }
            if ((attr == "channel") || (attr == "url"))
            {
                return UnresolvedChannel::parse(val).transform(  //
                    [&](UnresolvedChannel&& uc) { spec.set_channel(std::move(uc)); }
                );
            }
            if (attr == "subdir")
            {
                if (auto chan = spec.channel(); !chan.has_value() || chan->platform_filters().empty())
                {
                    spec.set_subdirs({ UnresolvedChannel::parse_platform_list(val) });
                }
                return {};
            }
            if ((attr == "fn") || (attr == "filename"))
            {
                spec.set_filename(std::string(val));
                return {};
            }
            if (attr == "md5")
            {
                spec.set_md5(std::string(val));
                return {};
            }
            if (attr == "sha256")
            {
                spec.set_sha256(std::string(val));
                return {};
            }
            if (attr == "license")
            {
                spec.set_license(std::string(val));
                return {};
            }
            if (attr == "license_family")
            {
                spec.set_license_family(std::string(val));
                return {};
            }
            if (attr == "features")
            {
                spec.set_features(std::string(val));
                return {};
            }
            if (attr == "track_features")
            {
                spec.set_track_features(std::string(val));
                return {};
            }
            if (attr == "optional")
            {
                spec.set_optional(is_true_string(val));
                return {};
            }
            // Don't fail to parse extra name to avoid failure on non-supported attributes
            return {};
        }

        [[nodiscard]] auto set_single_matchspec_attribute(  //
            MatchSpec& spec,
            std::string_view attr,
            std::string_view val
        ) -> expected_parse_t<void>
        {
            return set_single_matchspec_attribute_impl(spec, attr, val)
                .transform_error(
                    [&](ParseError&& err)
                    {
                        return ParseError(fmt::format(
                            R"(Error setting attribute "{}" to value "{}": {})",
                            attr,
                            val,
                            err.what()
                        ));
                    }
                );
        }

        [[nodiscard]] auto set_matchspec_attributes(  //
            MatchSpec& spec,
            std::string_view attrs
        ) -> expected_parse_t<void>
        {
            return find_attribute_split(attrs)
                .and_then(
                    [&](std::size_t next_pos)
                    {
                        auto [key, value] = util::split_once(
                            attrs.substr(0, next_pos),
                            MatchSpec::attribute_assign
                        );

                        return set_single_matchspec_attribute(
                                   spec,
                                   util::to_lower(util::strip(key)),
                                   strip_whitespace_quotes(value.value_or("true"))
                        )
                            .transform([&]() { return next_pos; });
                    }
                )
                .and_then(
                    [&](std::size_t next_pos) -> expected_parse_t<void>
                    {
                        if (next_pos != std::string_view::npos)
                        {
                            return set_matchspec_attributes(spec, attrs.substr(next_pos + 1));
                        }
                        return {};
                    }
                );
        }

        auto rfind_attribute_section(  //
            std::string_view str
        ) -> expected_parse_t<std::pair<std::size_t, std::size_t>>
        {
            return util::rfind_matching_parentheses(str, open_or_quote_tokens, close_or_quote_tokens)
                .map_error(
                    [&](const auto&)
                    { return ParseError(fmt::format(R"(Parentheses mismatch in "{}".)", str)); }
                );
        }

        auto rparse_and_set_matchspec_attributes(MatchSpec& spec, std::string_view str)
            -> expected_parse_t<std::string_view>
        {
            // Parsing all attributes sections backwards, for instance in
            // ``conda-forge::foo[build=3](target=blarg,optional)``
            // this results in:
            //   - ``target=blarg,optional``
            //   - ``build=3``

            if (!util::ends_with(str, MatchSpec::prefered_list_close)
                && !util::ends_with(str, MatchSpec::alt_list_close))
            {
                return str;
            }

            return rfind_attribute_section(util::rstrip(str))
                .and_then(
                    [&](auto start_end)
                    {
                        auto [start, end] = start_end;
                        assert(start != std::string::npos);
                        assert(end != std::string::npos);
                        assert(start < end);
                        return set_matchspec_attributes(spec, str.substr(start + 1, end - start - 1))
                            .and_then(  //
                                [&]() {
                                    return rparse_and_set_matchspec_attributes(
                                        spec,
                                        str.substr(0, start_end.first)
                                    );
                                }
                            );
                    }
                );
        }

        auto split_version_and_build(std::string_view str)
            -> std::pair<std::string_view, std::string_view>
        {
            str = util::strip(str);

            // Support faulty conda matchspecs such as `libblas=[build=*mkl]`, which is
            // the repr of `libblas=*=*mkl`
            str = util::rstrip(str, '=');

            const auto pos = str.find_last_of(" =");
            if (pos == str.npos || pos == 0)
            {
                return { str, {} };
            }

            if (char c = str[pos]; c == '=')
            {
                char d = str[pos - 1];
                if (d == '=' || d == '!' || d == '|' || d == ',' || d == '<' || d == '>' || d == '~')
                {
                    return { str, {} };
                }
            }

            // c is either ' ' or d is none of the forbidden chars
            return { str.substr(0, pos), str.substr(pos + 1) };
        }

        auto split_name_version_and_build(std::string_view str)
        {
            // Split the package name and version in ``pkg 1.5`` or ``pkg>=1.3=bld``.
            auto [pkg_name, version_and_build] = util::lstrip_if_parts(
                str,
                [](char c) -> bool { return !contains(MatchSpec::package_version_sep, c); }
            );

            auto [version_str, build_string_str] = split_version_and_build(version_and_build);
            return std::tuple(pkg_name, version_str, build_string_str);
        }
    }

    auto MatchSpec::parse(std::string_view str) -> expected_parse_t<MatchSpec>
    {
        auto parse_error = [&str](std::string_view err) -> tl::unexpected<ParseError>
        {
            return tl::make_unexpected(
                ParseError(fmt::format(R"(Error parsing MatchSpec "{}": {}")", str, err))
            );
        };

        static constexpr auto npos = std::string_view::npos;
        str = util::strip(str);
        if (str.empty())
        {
            return {};
        }

        // A plain URL like https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda
        if (has_archive_extension(str))
        {
            return MatchSpec::parse_url(str);
        }

        // A URL with hash, generated by `mamba env export --explicit` like
        // https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda#7dbaa197d7ba6032caf7ae7f32c1efa0
        if (const auto idx = str.rfind(url_md5_sep); idx != npos)
        {
            auto url = str.substr(0, idx);
            auto hash = str.substr(idx + 1);
            if (has_archive_extension(url))
            {
                return MatchSpec::parse_url(url).transform(
                    [&](MatchSpec&& ms) -> MatchSpec
                    {
                        if (is_hash(hash))
                        {
                            ms.set_md5(std::string(hash));
                        }
                        return ms;
                    }
                );
            }
        }

        auto out = MatchSpec();

        // Split full matchspec like
        // ``https://channel[plat]:namespace:spec >=3 [attr="val", ...]``
        // into:
        //   - ``https://channel[plat]``
        //   - ``namespace``
        //   - ``spec >=3 [attr="val", ...]``
        {
            auto maybe_chan_ns_spec = split_channel_namespace_spec(str);
            if (!maybe_chan_ns_spec)
            {
                return parse_error(maybe_chan_ns_spec.error().what());
            }

            auto [chan_str, ns_str, spec_str] = maybe_chan_ns_spec.value();

            out.set_name_space(std::string(ns_str));

            if (!chan_str.empty())
            {
                auto maybe_chan = UnresolvedChannel::parse(chan_str);
                if (!maybe_chan)
                {
                    return parse_error(maybe_chan.error().what());
                }
                out.m_channel = std::move(maybe_chan).value();
            }

            str = spec_str;
        }

        // Parse and apply bracket attributes ``attr="val"`` in ``pkg >=3 =mkl [attr="val", ...]``.
        // Split name "pkg", version ">=3" and build string "=mkl" left in string ``pkg >=3 =mkl``.
        auto name_str = std::string_view();
        auto ver_str = std::string_view();
        auto bld_str = std::string_view();
        {
            auto maybe_pkg_ver_bld = rparse_and_set_matchspec_attributes(out, str);
            if (!maybe_pkg_ver_bld)
            {
                return parse_error(maybe_pkg_ver_bld.error().what());
            }

            std::tie(name_str, ver_str, bld_str) = split_name_version_and_build(
                util::lstrip(std::move(maybe_pkg_ver_bld).value())
            );
        }

        // Set non-empty package name
        if (name_str.empty())
        {
            return parse_error("Empty package name.");
        }
        out.m_name = NameSpec(std::string(name_str));

        // Set the version and build string, but avoid overriding in case nothing is specified
        // as it may already be set in attribute as in ``numpy[version=1.12]``.
        if (!ver_str.empty())
        {
            auto maybe_ver = VersionSpec::parse(ver_str);
            if (!maybe_ver)
            {
                return parse_error(maybe_ver.error().what());
            }
            out.m_version = std::move(maybe_ver).value();
        }

        if (!bld_str.empty())
        {
            out.m_build_string = MatchSpec::BuildStringSpec(std::string(bld_str));
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
            return MatchSpec::parse({ str, len })
                .or_else([](specs::ParseError&& err) { throw std::move(err); })
                .value();
        }
    }
}
