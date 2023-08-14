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

#include <curl/urlapi.h>
#include <fmt/format.h>

#include "mamba/util/cast.hpp"
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
            explicit CURLStr(pointer data);
            explicit CURLStr(pointer data, size_type len);
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

        CURLStr::CURLStr(pointer data)
            : m_data(data)
        {
        }

        CURLStr::CURLStr(pointer data, size_type len)
            : m_data(data)
            , m_len(len)
        {
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

    // TODO should live in url_manip.hpp but stays here for simplicity of calling CURL
    auto url_encode(std::string_view url) -> std::string
    {
        auto output = CURLStr(
            curl_easy_escape(nullptr, url.data(), util::safe_num_cast<int>(url.size()))
        );
        if (auto str = output.str())
        {
            return std::string(*str);
        }
        throw std::runtime_error("Could not url-escape string.");
    }

    auto url_decode(std::string_view url) -> std::string
    {
        int out_length;
        char* output = curl_easy_unescape(
            nullptr,
            url.data(),
            util::safe_num_cast<int>(url.size()),
            &out_length
        );
        auto curl_str = CURLStr(output, out_length);
        if (auto str = curl_str.str())
        {
            return std::string(*str);
        }
        throw std::runtime_error("Could not url-unescape string.");
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
            out.set_scheme(handle.get_part(CURLUPART_SCHEME).value_or(std::string(URL::https)))
                .set_user(handle.get_part(CURLUPART_USER).value_or(""))
                .set_password(handle.get_part(CURLUPART_PASSWORD).value_or(""))
                .set_host(handle.get_part(CURLUPART_HOST).value_or(std::string(URL::localhost)))
                .set_path(handle.get_part(CURLUPART_PATH).value_or("/"))
                .set_port(handle.get_part(CURLUPART_PORT).value_or(""))
                .set_query(handle.get_part(CURLUPART_QUERY).value_or(""))
                .set_fragment(handle.get_part(CURLUPART_FRAGMENT).value_or(""));
        }
        return out;
    }

    auto URL::scheme() const -> const std::string&
    {
        return m_scheme;
    }

    auto URL::set_scheme(std::string_view scheme) -> URL&
    {
        if (scheme.empty())
        {
            throw std::invalid_argument("Cannot set empty scheme");
        }
        m_scheme = util::to_lower(util::rstrip(scheme));
        return *this;
    }

    auto URL::user() const -> const std::string&
    {
        return m_user;
    }

    auto URL::set_user(std::string_view user) -> URL&
    {
        m_user = user;
        return *this;
    }

    auto URL::password() const -> const std::string&
    {
        return m_password;
    }

    auto URL::set_password(std::string_view password) -> URL&
    {
        m_password = password;
        return *this;
    }

    auto URL::authentication() const -> std::string
    {
        const auto& u = user();
        const auto& p = password();
        return p.empty() ? u : util::concat(u, ':', p);
    }

    auto URL::host() const -> const std::string&
    {
        return m_host;
    }

    auto URL::set_host(std::string_view host) -> URL&
    {
        if (host.empty())
        {
            throw std::invalid_argument("Cannot set empty host");
        }
        m_host = util::to_lower(util::rstrip(host));
        return *this;
    }

    auto URL::port() const -> const std::string&
    {
        return m_port;
    }

    auto URL::set_port(std::string_view port) -> URL&
    {
        if (!std::all_of(port.cbegin(), port.cend(), [](char c) { return util::is_digit(c); }))
        {
            throw std::invalid_argument(fmt::format(R"(Port must be a number, got "{}")", port));
        }
        m_port = port;
        return *this;
    }

    auto URL::authority() const -> std::string
    {
        return util::concat(
            m_user,
            m_password.empty() ? "" : ":",
            m_password,
            m_user.empty() ? "" : "@",
            m_host,
            m_port.empty() ? "" : ":",
            m_port
        );
    }

    auto URL::path() const -> const std::string&
    {
        return m_path;
    }

    auto URL::pretty_path() const -> std::string_view
    {
        // All paths start with a '/' except those like "file://C:/folder/file.txt"
        if (m_scheme == "file")
        {
            assert(util::starts_with(m_path, '/'));
            auto path_no_slash = std::string_view(m_path).substr(1);
            if (path_has_drive_letter(path_no_slash))
            {
                return path_no_slash;
            }
        }
        return m_path;
    }

    auto URL::set_path(std::string_view path) -> URL&
    {
        if (!util::starts_with(path, '/'))
        {
            m_path.reserve(path.size() + 1);
            m_path = '/';
            m_path += path;
        }
        else
        {
            m_path = path;
        }
        return *this;
    }

    auto URL::append_path(std::string_view subpath) -> URL&
    {
        subpath = util::strip(subpath);
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
        return *this;
    }

    auto URL::query() const -> const std::string&
    {
        return m_query;
    }

    auto URL::set_query(std::string_view query) -> URL&
    {
        m_query = query;
        return *this;
    }

    auto URL::fragment() const -> const std::string&
    {
        return m_fragment;
    }

    auto URL::set_fragment(std::string_view fragment) -> URL&
    {
        m_fragment = fragment;
        return *this;
    }

    auto URL::str(StripScheme strip) const -> std::string
    {
        // Not showing "localhost" on file URI
        std::string_view computed_host = m_host;
        if ((m_scheme == "file") && (m_host == localhost))
        {
            computed_host = "";
        }
        // When stripping file scheme, not showing leading '/' for Windows path with drive
        std::string_view computed_path = m_path;
        if ((m_scheme == "file") && (strip == StripScheme::yes) && computed_host.empty())
        {
            computed_path = pretty_path();
        }
        return util::concat(
            (strip == StripScheme::no) ? m_scheme : "",
            (strip == StripScheme::no) ? "://" : "",
            m_user,
            m_password.empty() ? "" : ":",
            m_password,
            m_user.empty() ? "" : "@",
            computed_host,
            m_port.empty() ? "" : ":",
            m_port,
            computed_path,
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
        return URL(std::move(url));
    }

}  // namespace mamba
