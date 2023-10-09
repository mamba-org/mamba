// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <curl/urlapi.h>
#include <fmt/format.h>

#include "mamba/util/build.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
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
        class CURLUrl
        {
        public:

            using value_type = ::CURLU;
            using pointer = value_type*;
            using const_pointer = const value_type*;
            using flag_type = unsigned int;

            CURLUrl();
            CURLUrl(const std::string& url, flag_type flags = 0);
            ~CURLUrl();

            CURLUrl(const CURLUrl&) = delete;
            CURLUrl& operator=(const CURLUrl&) = delete;
            CURLUrl(CURLUrl&&) = delete;
            CURLUrl& operator=(CURLUrl&&) = delete;

            [[nodiscard]] auto get_part(CURLUPart part, flag_type flags = 0) const
                -> std::optional<std::string>;

        private:

            pointer m_handle = nullptr;
        };

        /**
         * A RAII wrapper for string mananged by CURL.
         *
         * String can possibly be null, or zero-lenght, depending on the data returned by CURL.
         */
        class CURLStr
        {
            using value_type = char;
            using pointer = value_type*;
            using const_pointer = const value_type*;
            using input_pointer = value_type**;
            using size_type = int;

        public:

            explicit CURLStr() = default;
            ~CURLStr();

            CURLStr(const CURLStr&) = delete;
            CURLStr& operator=(const CURLStr&) = delete;
            CURLStr(CURLStr&&) = delete;
            CURLStr& operator=(CURLStr&&) = delete;

            [[nodiscard]] auto raw_input() -> input_pointer;

            [[nodiscard]] auto str() const -> std::optional<std::string_view>;

        private:

            pointer m_data = nullptr;
            // Only meaningful when > 0, otherwise, assume null terminated string
            size_type m_len = { -1 };
        };

        CURLUrl::CURLUrl()
        {
            m_handle = ::curl_url();
            if (m_handle == nullptr)
            {
                throw std::runtime_error("Could not create CURLU handle");
            }
        }

        CURLUrl::CURLUrl(const std::string& url, flag_type flags)
            : CURLUrl()
        {
            const CURLUcode uc = ::curl_url_set(m_handle, CURLUPART_URL, url.c_str(), flags);
            if (uc != CURLUE_OK)
            {
                throw std::invalid_argument(
                    fmt::format(R"(Failed to parse URL "{}": {})", url, ::curl_url_strerror(uc))
                );
            }
        }

        CURLUrl::~CURLUrl()
        {
            ::curl_url_cleanup(m_handle);
        }

        auto CURLUrl::get_part(CURLUPart part, flag_type flags) const -> std::optional<std::string>
        {
            CURLStr scheme{};
            const auto rc = ::curl_url_get(m_handle, part, scheme.raw_input(), flags);
            if (!rc)
            {
                if (auto str = scheme.str())
                {
                    return std::string(*str);
                }
            }
            return std::nullopt;
        }

        CURLStr::~CURLStr()
        {
            // Even when Curl returns a len along side the data, `curl_free` must be used without
            // len.
            ::curl_free(m_data);
            m_data = nullptr;
        }

        auto CURLStr::raw_input() -> input_pointer
        {
            assert(m_data == nullptr);  // Otherwise we leak Curl memory
            return &m_data;
        }

        auto CURLStr::str() const -> std::optional<std::string_view>
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

    auto URL::parse(std::string_view url) -> URL
    {
        url = util::rstrip(url);
        auto out = URL();
        if (!url.empty())
        {
            // CURL fails to parse the URL if no scheme is given, unless CURLU_DEFAULT_SCHEME is
            // given
            const CURLUrl handle = {
                file_uri_unc2_to_unc4(url),
                CURLU_NON_SUPPORT_SCHEME | CURLU_DEFAULT_SCHEME,
            };
            out.set_scheme(handle.get_part(CURLUPART_SCHEME).value_or(""));
            out.set_user(handle.get_part(CURLUPART_USER).value_or(""), Encode::no);
            out.set_password(handle.get_part(CURLUPART_PASSWORD).value_or(""), Encode::no);
            out.set_host(handle.get_part(CURLUPART_HOST).value_or(""));
            out.set_path(handle.get_part(CURLUPART_PATH).value_or("/"));
            out.set_port(handle.get_part(CURLUPART_PORT).value_or(""));
            out.set_query(handle.get_part(CURLUPART_QUERY).value_or(""));
            out.set_fragment(handle.get_part(CURLUPART_FRAGMENT).value_or(""));
        }
        return out;
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

    auto URL::user(Decode::no_type) const -> const std::string&
    {
        return m_user;
    }

    auto URL::user(Decode::yes_type) const -> std::string
    {
        return url_decode(user(Decode::no));
    }

    void URL::set_user(std::string_view user, Encode::yes_type)
    {
        return set_user(url_encode(user), Encode::no);
    }

    void URL::set_user(std::string user, Encode::no_type)
    {
        m_user = std::move(user);
    }

    auto URL::clear_user() -> std::string
    {
        return std::exchange(m_user, "");
    }

    auto URL::password(Decode::no_type) const -> const std::string&
    {
        return m_password;
    }

    auto URL::password(Decode::yes_type) const -> std::string
    {
        return url_decode(password(Decode::no));
    }

    void URL::set_password(std::string_view password, Encode::yes_type)
    {
        return set_password(url_encode(password), Encode::no);
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
        authentication_impl(URL::Credentials credentials, UGetter&& get_user, PGetter&& get_password)
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

    auto URL::authentication(Credentials credentials, Decode::no_type) const
        -> std::array<std::string_view, 3>
    {
        return authentication_impl<std::string_view>(
            credentials,
            [&]() -> std::string_view { return user(Decode::no); },
            [&]() -> std::string_view { return password(Decode::no); }
        );
    }

    auto URL::authentication(Credentials credentials, Decode::yes_type) const
        -> std::array<std::string, 3>
    {
        return authentication_impl<std::string>(
            credentials,
            [&]() -> std::string { return user(Decode::yes); },
            [&]() -> std::string { return password(Decode::yes); }
        );
    }

    auto URL::authentication() const -> std::string
    {
        auto user_sep_pass = authentication(Credentials::Show, Decode::no);
        return util::concat(user_sep_pass[0], user_sep_pass[1], user_sep_pass[2]);
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
        return url_decode(host(Decode::no));
    }

    void URL::set_host(std::string_view host, Encode::yes_type)
    {
        return set_host(url_encode(host), Encode::no);
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

    auto URL::authority() const -> std::string
    {
        const auto& l_user = user(Decode::no);
        const auto& l_pass = password(Decode::no);
        const auto& l_host = host(Decode::no);
        return util::concat(
            l_user,
            l_pass.empty() ? "" : ":",
            l_pass,
            l_user.empty() ? "" : "@",
            l_host,
            m_port.empty() ? "" : ":",
            m_port
        );
    }

    auto URL::path(Decode::no_type) const -> const std::string&
    {
        return m_path;
    }

    auto URL::path(Decode::yes_type) const -> std::string
    {
        return url_decode(path(Decode::no));
    }

    void URL::set_path(std::string_view path, Encode::yes_type)
    {
        // Drive colon must not be encoded
        if (on_win && (scheme() == "file"))
        {
            auto [slashes, no_slash_path] = lstrip_parts(path, '/');
            if (slashes.empty())
            {
                slashes = "/";
            }
            if ((no_slash_path.size() >= 2) && path_has_drive_letter(no_slash_path))
            {
                m_path = concat(
                    slashes,
                    no_slash_path.substr(0, 2),
                    url_encode(no_slash_path.substr(2), '/')
                );
            }
            else
            {
                m_path = concat(slashes, url_encode(no_slash_path, '/'));
            }
        }
        else
        {
            return set_path(url_encode(path, '/'), Encode::no);
        }
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
        if (scheme() == "file")
        {
            assert(util::starts_with(m_path, '/'));
            auto path_no_slash = url_decode(std::string_view(m_path).substr(1));
            if (path_has_drive_letter(path_no_slash))
            {
                return path_no_slash;
            }
        }
        return url_decode(m_path);
    }

    void URL::append_path(std::string_view subpath, Encode::yes_type)
    {
        if (path(Decode::no) == "/")
        {
            // Allow hanldling of Windows drive letter encoding
            return set_path(std::string(subpath), Encode::yes);
        }
        return append_path(url_encode(subpath, '/'), Encode::no);
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
        auto user_sep_pass = authentication(credentials, Decode::no);
        return util::concat(
            scheme(),
            "://",
            user_sep_pass[0],
            user_sep_pass[1],
            user_sep_pass[2],
            user_sep_pass[0].empty() ? "" : "@",
            host(Decode::no),
            m_port.empty() ? "" : ":",
            port(),
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
        auto user_sep_pass = authentication(credentials, Decode::yes);
        return util::concat(
            (strip_scheme == StripScheme::no) ? scheme() : "",
            (strip_scheme == StripScheme::no) ? "://" : "",
            user_sep_pass[0],
            user_sep_pass[1],
            user_sep_pass[2],
            user_sep_pass[0].empty() ? "" : "@",
            host(Decode::yes),
            m_port.empty() ? "" : ":",
            m_port,
            pretty_str_path(strip_scheme, rstrip_path),
            m_query.empty() ? "" : "?",
            m_query,
            m_fragment.empty() ? "" : "#",
            m_fragment
        );
    }

    auto operator==(URL const& a, URL const& b) -> bool
    {
        return (a.scheme() == b.scheme())
               && (a.user() == b.user())
               // omitting password, is that desirable?
               && (a.host() == b.host())
               // Would it be desirable to account for default ports?
               && (a.port() == b.port()) && (a.path() == b.path()) && (a.query() == b.query())
               && (a.fragment() == b.fragment());
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
