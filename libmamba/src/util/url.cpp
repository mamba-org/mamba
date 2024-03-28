// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include <curl/urlapi.h>
#include <fmt/format.h>

#include "mamba/util/build.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/tuple_hash.hpp"
#include "mamba/util/url.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::util
{

    /*******************
     *  CURL wrappers  *
     *******************/

    namespace
    {
        /**
         * A RAII ``CURLU*`` created from ``curl_url``.
         *
         * Never null, throw exception at construction if creating the handle fails.
         */
        class CurlUrl
        {
        public:

            using value_type = ::CURLU;
            using pointer = value_type*;
            using const_pointer = const value_type*;
            using flag_type = unsigned int;

            static auto parse(const std::string& url, flag_type flags = 0)
                -> tl::expected<CurlUrl, URL::ParseError>;

            CurlUrl();

            [[nodiscard]] auto
            get_part(::CURLUPart part, flag_type flags = 0) const -> std::optional<std::string>;

        private:

            struct CurlDeleter
            {
                void operator()(pointer ptr);
            };

            std::unique_ptr<value_type, CurlDeleter> m_handle = nullptr;
        };

        /**
         * A RAII wrapper for string mananged by CURL.
         *
         * String can possibly be null, or zero-lenght, depending on the data returned by CURL.
         */
        class CurlStr
        {
            using value_type = char;
            using pointer = value_type*;
            using const_pointer = const value_type*;
            using input_pointer = value_type**;
            using size_type = int;

        public:

            explicit CurlStr() = default;
            ~CurlStr();

            CurlStr(const CurlStr&) = delete;
            auto operator=(const CurlStr&) -> CurlStr& = delete;
            CurlStr(CurlStr&&) = delete;
            auto operator=(CurlStr&&) -> CurlStr& = delete;

            [[nodiscard]] auto raw_input() -> input_pointer;

            [[nodiscard]] auto str() const -> std::optional<std::string_view>;

        private:

            pointer m_data = nullptr;
            // Only meaningful when > 0, otherwise, assume null terminated string
            size_type m_len = { -1 };
        };

        auto
        CurlUrl::parse(const std::string& url, flag_type flags) -> tl::expected<CurlUrl, URL::ParseError>
        {
            auto out = CurlUrl();
            const CURLUcode uc = ::curl_url_set(out.m_handle.get(), CURLUPART_URL, url.c_str(), flags);
            if (uc != CURLUE_OK)
            {
                return tl::make_unexpected(URL::ParseError{
                    fmt::format(R"(Failed to parse URL "{}": {})", url, ::curl_url_strerror(uc)) });
            }
            return { std::move(out) };
        }

        CurlUrl::CurlUrl()
        {
            m_handle.reset(::curl_url());
            if (m_handle == nullptr)
            {
                throw std::runtime_error("Could not create CurlUrl handle");
            }
        }

        void CurlUrl::CurlDeleter::operator()(pointer ptr)
        {
            if (ptr)
            {
                ::curl_url_cleanup(ptr);
            }
        }

        auto CurlUrl::get_part(CURLUPart part, flag_type flags) const -> std::optional<std::string>
        {
            CurlStr scheme{};
            const auto rc = ::curl_url_get(m_handle.get(), part, scheme.raw_input(), flags);
            if (!rc)
            {
                if (auto str = scheme.str())
                {
                    return std::string(*str);
                }
            }
            return std::nullopt;
        }

        CurlStr::~CurlStr()
        {
            // Even when Curl returns a len along side the data, `curl_free` must be used without
            // len.
            ::curl_free(m_data);
            m_data = nullptr;
        }

        auto CurlStr::raw_input() -> input_pointer
        {
            assert(m_data == nullptr);  // Otherwise we leak Curl memory
            return &m_data;
        }

        auto CurlStr::str() const -> std::optional<std::string_view>
        {
            if (m_data)
            {
                if (m_len > 0)
                {
                    return { { m_data, static_cast<std::size_t>(m_len) } };
                }
                else
                {
                    return { { m_data } };
                }
            }
            return std::nullopt;
        }
    }

    /*****************************
     * URLHandler implementation *
     *****************************/

    auto URL::parse(std::string_view url) -> tl::expected<URL, ParseError>
    {
        url = util::rstrip(url);
        if (url.empty())
        {
            return tl::make_unexpected(ParseError{ "Empty URL" });
        }
        return CurlUrl::parse(file_uri_unc2_to_unc4(url), CURLU_NON_SUPPORT_SCHEME | CURLU_DEFAULT_SCHEME)
            .transform(
                [&](CurlUrl&& handle) -> URL
                {
                    auto out = URL();
                    // CURL fails to parse the URL if no scheme is given, unless
                    // CURLU_DEFAULT_SCHEME is given
                    out.set_scheme(handle.get_part(CURLUPART_SCHEME).value_or(""));
                    out.set_user(handle.get_part(CURLUPART_USER).value_or(""), Encode::no);
                    out.set_password(handle.get_part(CURLUPART_PASSWORD).value_or(""), Encode::no);
                    out.set_host(handle.get_part(CURLUPART_HOST).value_or(""), Encode::no);
                    out.set_path(handle.get_part(CURLUPART_PATH).value_or("/"), Encode::no);
                    out.set_port(handle.get_part(CURLUPART_PORT).value_or(""));
                    out.set_query(handle.get_part(CURLUPART_QUERY).value_or(""));
                    out.set_fragment(handle.get_part(CURLUPART_FRAGMENT).value_or(""));
                    return out;
                }
            );
    }

    auto URL::scheme_is_defaulted() const -> bool
    {
        return m_scheme.empty();
    }

    auto URL::scheme() const -> std::string_view
    {
        if (scheme_is_defaulted())
        {
            return https;
        }
        return m_scheme;
    }

    void URL::set_scheme(std::string_view scheme)
    {
        m_scheme = util::to_lower(util::rstrip(scheme));
    }

    auto URL::clear_scheme() -> std::string
    {
        if (scheme_is_defaulted())
        {
            return std::string(https);
        }
        return std::exchange(m_scheme, "");
    }

    auto URL::has_user() const -> bool
    {
        return !m_user.empty();
    }

    auto URL::user(Decode::no_type) const -> const std::string&
    {
        return m_user;
    }

    auto URL::user(Decode::yes_type) const -> std::string
    {
        return decode_percent(user(Decode::no));
    }

    void URL::set_user(std::string_view user, Encode::yes_type)
    {
        return set_user(encode_percent(user), Encode::no);
    }

    void URL::set_user(std::string user, Encode::no_type)
    {
        m_user = std::move(user);
    }

    auto URL::clear_user() -> std::string
    {
        return std::exchange(m_user, "");
    }

    auto URL::has_password() const -> bool
    {
        return !m_password.empty();
    }

    auto URL::password(Decode::no_type) const -> const std::string&
    {
        return m_password;
    }

    auto URL::password(Decode::yes_type) const -> std::string
    {
        return decode_percent(password(Decode::no));
    }

    void URL::set_password(std::string_view password, Encode::yes_type)
    {
        return set_password(encode_percent(password), Encode::no);
    }

    void URL::set_password(std::string password, Encode::no_type)
    {
        m_password = std::move(password);
    }

    auto URL::clear_password() -> std::string
    {
        return std::exchange(m_password, "");
    }

    namespace
    {
        template <typename Str, typename UGetter, typename PGetter>
        auto
        authentication_elems_impl(URL::Credentials credentials, UGetter&& get_user, PGetter&& get_password)
        {
            switch (credentials)
            {
                case (URL::Credentials::Show):
                {
                    Str user = get_user();
                    Str pass = user.empty() ? "" : get_password();
                    Str sep = pass.empty() ? "" : ":";
                    return std::array<Str, 3>{ std::move(user), std::move(sep), std::move(pass) };
                }
                case (URL::Credentials::Hide):
                {
                    Str user = get_user();
                    Str pass = user.empty() ? "" : "*****";
                    Str sep = user.empty() ? "" : ":";
                    return std::array<Str, 3>{ std::move(user), std::move(sep), std::move(pass) };
                }
                case (URL::Credentials::Remove):
                {
                    return std::array<Str, 3>{ "", "", "" };
                }
            }
            assert(false);
            throw std::invalid_argument("Invalid enum number");
        }
    }

    auto URL::authentication_elems(Credentials credentials, Decode::no_type) const
        -> std::array<std::string_view, 3>
    {
        return authentication_elems_impl<std::string_view>(
            credentials,
            [&]() -> std::string_view { return user(Decode::no); },
            [&]() -> std::string_view { return password(Decode::no); }
        );
    }

    auto URL::authentication_elems(Credentials credentials, Decode::yes_type) const
        -> std::array<std::string, 3>
    {
        return authentication_elems_impl<std::string>(
            credentials,
            [&]() -> std::string { return user(Decode::yes); },
            [&]() -> std::string { return password(Decode::yes); }
        );
    }

    auto URL::authentication() const -> std::string
    {
        return std::apply(
            [](auto&&... elem) { return util::concat(std::forward<decltype(elem)>(elem)...); },
            authentication_elems(Credentials::Show, Decode::no)
        );
    }

    auto URL::host_is_defaulted() const -> bool
    {
        return m_host.empty();
    }

    auto URL::host(Decode::no_type) const -> std::string_view
    {
        if ((scheme() != "file") && host_is_defaulted())
        {
            return localhost;
        }
        return m_host;
    }

    auto URL::host(Decode::yes_type) const -> std::string
    {
        return decode_percent(host(Decode::no));
    }

    void URL::set_host(std::string_view host, Encode::yes_type)
    {
        return set_host(encode_percent(host), Encode::no);
    }

    void URL::set_host(std::string host, Encode::no_type)
    {
        std::transform(
            host.cbegin(),
            host.cend(),
            host.begin(),
            [](char c) { return util::to_lower(c); }
        );
        m_host = std::move(host);
    }

    auto URL::clear_host() -> std::string
    {
        if (host_is_defaulted())
        {
            return std::string(host(Decode::no));
        }
        return std::exchange(m_host, "");
    }

    auto URL::port() const -> const std::string&
    {
        return m_port;
    }

    void URL::set_port(std::string_view port)
    {
        if (!std::all_of(port.cbegin(), port.cend(), [](char c) { return util::is_digit(c); }))
        {
            throw std::invalid_argument(fmt::format(R"(Port must be a number, got "{}")", port));
        }
        m_port = port;
    }

    auto URL::clear_port() -> std::string
    {
        return std::exchange(m_port, "");
    }

    namespace
    {
        template <typename Str>
        auto authority_elems_impl(std::array<Str, 3> user_sep_pass, Str host, Str port)
        {
            const bool has_auth = !user_sep_pass[0].empty();
            const bool has_port = !port.empty();
            return std::array<Str, 7>{
                std::move(user_sep_pass[0]),
                std::move(user_sep_pass[1]),
                std::move(user_sep_pass[2]),
                Str(has_auth ? "@" : ""),
                std::move(host),
                Str(has_port ? ":" : ""),
                std::move(port),
            };
        }
    }

    auto URL::authority_elems(Credentials credentials, Decode::no_type) const
        -> std::array<std::string_view, 7>
    {
        return authority_elems_impl<std::string_view>(
            authentication_elems(credentials, Decode::no),
            host(Decode::no),
            port()
        );
    }

    auto
    URL::authority_elems(Credentials credentials, Decode::yes_type) const -> std::array<std::string, 7>
    {
        return authority_elems_impl<std::string>(
            authentication_elems(credentials, Decode::yes),
            host(Decode::yes),
            port()
        );
    }

    auto URL::authority(Credentials credentials) const -> std::string
    {
        return std::apply(
            [](auto&&... elem) { return util::concat(std::forward<decltype(elem)>(elem)...); },
            authority_elems(credentials, Decode::no)
        );
    }

    auto URL::path(Decode::no_type) const -> const std::string&
    {
        return m_path;
    }

    auto URL::path(Decode::yes_type) const -> std::string
    {
        return decode_percent(path(Decode::no));
    }

    void URL::set_path(std::string_view path, Encode::yes_type)
    {
        // Drive colon must not be encoded
        if (on_win && (scheme() == "file"))
        {
            auto [slashes, no_slash_path] = lstrip_parts(path, '/');
            if ((no_slash_path.size() >= 2) && path_has_drive_letter(no_slash_path))
            {
                return set_path(
                    concat(
                        slashes.empty() ? "/" : slashes,
                        no_slash_path.substr(0, 2),
                        encode_percent(no_slash_path.substr(2), '/')
                    ),
                    Encode::no
                );
            }
        }
        return set_path(encode_percent(path, '/'), Encode::no);
    }

    void URL::set_path(std::string path, Encode::no_type)
    {
        if (!util::starts_with(path, '/'))
        {
            path.insert(0, 1, '/');
        }
        m_path = path;
    }

    auto URL::clear_path() -> std::string
    {
        return std::exchange(m_path, "/");
    }

    auto URL::pretty_path() const -> std::string
    {
        // All paths start with a '/' except those like "file:///C:/folder/file.txt"
        if (on_win && scheme() == "file")
        {
            assert(util::starts_with(m_path, '/'));
            auto path_no_slash = decode_percent(std::string_view(m_path).substr(1));
            if (path_has_drive_letter(path_no_slash))
            {
                return path_no_slash;
            }
        }
        return decode_percent(m_path);
    }

    void URL::append_path(std::string_view subpath, Encode::yes_type)
    {
        if (util::lstrip(path(Decode::no), '/').empty())
        {
            // Allow handling of Windows drive letter encoding
            return set_path(std::string(subpath), Encode::yes);
        }
        return append_path(encode_percent(subpath, '/'), Encode::no);
    }

    void URL::append_path(std::string_view subpath, Encode::no_type)
    {
        m_path.reserve(m_path.size() + 1 + subpath.size());
        const bool trailing = util::ends_with(m_path, '/');
        const bool leading = util::starts_with(subpath, '/');
        if (!trailing && !leading && !subpath.empty())
        {
            m_path += '/';
        }
        if (trailing && leading)
        {
            m_path.pop_back();
        }
        m_path += subpath;
    }

    auto URL::query() const -> const std::string&
    {
        return m_query;
    }

    void URL::set_query(std::string_view query)
    {
        m_query = query;
    }

    auto URL::clear_query() -> std::string
    {
        return std::exchange(m_query, "");
    }

    auto URL::fragment() const -> const std::string&
    {
        return m_fragment;
    }

    void URL::set_fragment(std::string_view fragment)
    {
        m_fragment = fragment;
    }

    auto URL::clear_fragment() -> std::string
    {
        return std::exchange(m_fragment, "");
    }

    auto URL::str(Credentials credentials) const -> std::string
    {
        std::array<std::string_view, 7> authority = authority_elems(credentials, Decode::no);
        return util::concat(
            scheme(),
            "://",
            authority[0],
            authority[1],
            authority[2],
            authority[3],
            authority[4],
            authority[5],
            authority[6],
            path(Decode::no),
            m_query.empty() ? "" : "?",
            m_query,
            m_fragment.empty() ? "" : "#",
            m_fragment
        );
    }

    auto URL::pretty_str_path(StripScheme strip_scheme, char rstrip_path) const -> std::string
    {
        std::string computed_path = {};
        // When stripping file scheme, not showing leading '/' for Windows path with drive
        if ((scheme() == "file") && (strip_scheme == StripScheme::yes) && host(Decode::no).empty())
        {
            computed_path = pretty_path();
        }
        else
        {
            computed_path = path(Decode::yes);
        }
        computed_path = util::rstrip(computed_path, rstrip_path);
        return computed_path;
    }

    auto URL::pretty_str(StripScheme strip_scheme, char rstrip_path, Credentials credentials) const
        -> std::string
    {
        std::array<std::string, 7> authority = authority_elems(credentials, Decode::yes);
        return util::concat(
            (strip_scheme == StripScheme::no) ? scheme() : "",
            (strip_scheme == StripScheme::no) ? "://" : "",
            authority[0],
            authority[1],
            authority[2],
            authority[3],
            authority[4],
            authority[5],
            authority[6],
            pretty_str_path(strip_scheme, rstrip_path),
            m_query.empty() ? "" : "?",
            m_query,
            m_fragment.empty() ? "" : "#",
            m_fragment
        );
    }

    namespace
    {
        auto attrs(URL const& url)
        {
            return std::tuple<
                std::string_view,
                const std::string&,
                const std::string&,
                std::string_view,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::string&>{
                url.scheme(),
                url.user(URL::Decode::no),
                url.password(URL::Decode::no),
                url.host(URL::Decode::no),
                url.port(),
                url.path(URL::Decode::no),
                url.query(),
                url.fragment(),
            };
        }
    }

    auto operator==(URL const& a, URL const& b) -> bool
    {
        return attrs(a) == attrs(b);
    }

    auto operator!=(URL const& a, URL const& b) -> bool
    {
        return !(a == b);
    }

    auto operator/(URL const& url, std::string_view subpath) -> URL
    {
        return URL(url) / subpath;
    }

    auto operator/(URL&& url, std::string_view subpath) -> URL
    {
        url.append_path(subpath);
        return std::move(url);
    }

}  // namespace mamba

auto
std::hash<mamba::util::URL>::operator()(const mamba::util::URL& u) const -> std::size_t
{
    return mamba::util::hash_tuple(mamba::util::attrs(u));
}
