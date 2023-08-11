// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>

#include <curl/urlapi.h>
#include <fmt/format.h>
#include <openssl/evp.h>

#include "mamba/core/context.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{

    /*******************
     *  CURL wrappers  *
     *******************/

    namespace
    {
        /**
         * A RAII ``CURL*`` created from ``curl_easy_handle``.
         *
         * Never null, throw exception at construction if creating the handle fails.
         * This wrapper is not meant to handle more cases, it remains an implementation detail
         * for url parsing, which we should ideally do without Curl.
         * This differs from ``CURLHanlde`` in "curl.hpp" which is a fully-featured CURL wrapper
         * for accessing the internet.
         */
        class CURLEasyHandle
        {
        public:

            using value_type = ::CURL;
            using pointer = value_type*;
            using const_pointer = const value_type*;

            CURLEasyHandle();
            ~CURLEasyHandle();

            CURLEasyHandle(const CURLEasyHandle&) = delete;
            CURLEasyHandle& operator=(const CURLEasyHandle&) = delete;
            CURLEasyHandle(CURLEasyHandle&&) = delete;
            CURLEasyHandle& operator=(CURLEasyHandle&&) = delete;

            [[nodiscard]] auto raw() -> pointer;

        private:

            pointer m_handle = nullptr;
        };

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

            CURLUrl(const CURLEasyHandle&) = delete;
            CURLUrl& operator=(const CURLEasyHandle&) = delete;
            CURLUrl(CURLEasyHandle&&) = delete;
            CURLUrl& operator=(CURLEasyHandle&&) = delete;

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

        CURLEasyHandle::CURLEasyHandle()
        {
            m_handle = ::curl_easy_init();
            if (m_handle == nullptr)
            {
                throw std::runtime_error("Could not create CURL handle");
            }
        }

        CURLEasyHandle::~CURLEasyHandle()
        {
            ::curl_easy_cleanup(m_handle);
        }

        auto CURLEasyHandle::raw() -> pointer
        {
            return m_handle;
        }

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

    /*********************
     * Utility functions *
     *********************/

    // proper file scheme on Windows is `file:///C:/blabla`
    // https://blogs.msdn.microsoft.com/ie/2006/12/06/file-uris-in-windows/
    std::string concat_scheme_url(const std::string& scheme, const std::string& location)
    {
        if (scheme == "file" && location.size() > 1 && location[1] == ':')
        {
            return util::concat("file:///", location);
        }
        else
        {
            return util::concat(scheme, "://", location);
        }
    }

    std::string build_url(
        const std::optional<std::string>& auth,
        const std::string& scheme,
        const std::string& base,
        bool with_credential
    )
    {
        if (with_credential && auth)
        {
            return concat_scheme_url(scheme, util::concat(*auth, "@", base));
        }
        else
        {
            return concat_scheme_url(scheme, base);
        }
    }

    void split_platform(
        const std::vector<std::string>& known_platforms,
        const std::string& url,
        const std::string& context_platform,
        std::string& cleaned_url,
        std::string& platform
    )
    {
        platform = "";

        auto check_platform_position = [&url](std::size_t pos, const std::string& lplatform) -> bool
        {
            if (pos == std::string::npos)
            {
                return false;
            }
            if (pos > 0 && url[pos - 1] != '/')
            {
                return false;
            }
            if ((pos + lplatform.size()) < url.size() && url[pos + lplatform.size()] != '/')
            {
                return false;
            }

            return true;
        };

        std::size_t pos = url.find(context_platform);
        if (check_platform_position(pos, context_platform))
        {
            platform = context_platform;
        }
        else
        {
            for (auto it = known_platforms.begin(); it != known_platforms.end(); ++it)
            {
                pos = url.find(*it);
                if (check_platform_position(pos, *it))
                {
                    platform = *it;
                    break;
                }
            }
        }

        cleaned_url = url;
        if (pos != std::string::npos)
        {
            cleaned_url.replace(pos - 1, platform.size() + 1, "");
        }
        cleaned_url = util::rstrip(cleaned_url, "/");
    }

    auto url_get_scheme(std::string_view url) -> std::string_view
    {
        static constexpr auto is_scheme_char = [](char c) -> bool
        { return is_alphanum(c) || (c == '.') || (c == '-') || (c == '_'); };

        const auto sep = url.find("://");
        if ((0 < sep) && (sep < std::string_view::npos))
        {
            auto scheme = url.substr(0, sep);
            if (std::all_of(scheme.cbegin(), scheme.cend(), is_scheme_char))
            {
                return scheme;
            }
        }
        return "";
    }

    auto url_has_scheme(std::string_view url) -> bool
    {
        return !url_get_scheme(url).empty();
    }

    auto path_has_drive_letter(std::string_view path) -> bool
    {
        static constexpr auto is_drive_char = [](char c) -> bool { return is_alphanum(c); };

        auto [drive, rest] = lstrip_if_parts(path, is_drive_char);
        return !drive.empty() && (rest.size() >= 2) && (rest[0] == ':')
               && ((rest[1] == '/') || (rest[1] == '\\'));
    }

    void split_anaconda_token(const std::string& url, std::string& cleaned_url, std::string& token)
    {
        auto token_begin = std::sregex_iterator(url.begin(), url.end(), Context::instance().token_regex);
        if (token_begin != std::sregex_iterator())
        {
            token = token_begin->str().substr(3u);
            cleaned_url = std::regex_replace(
                url,
                Context::instance().token_regex,
                "",
                std::regex_constants::format_first_only
            );
        }
        else
        {
            token = "";
            cleaned_url = url;
        }
        cleaned_url = util::rstrip(cleaned_url, "/");
    }

    void split_scheme_auth_token(
        const std::string& url,
        std::string& remaining_url,
        std::string& scheme,
        std::string& auth,
        std::string& token
    )
    {
        std::string cleaned_url;
        split_anaconda_token(url, cleaned_url, token);
        URL url_parsed = URL::parse(cleaned_url);
        scheme = url_parsed.scheme();
        auth = url_parsed.auth();
        url_parsed.set_scheme("");
        url_parsed.set_user("");
        url_parsed.set_password("");
        remaining_url = rstrip(url_parsed.str(), "/");
    }

    bool compare_cleaned_url(const std::string& url1, const std::string& url2)
    {
        std::string u1_remaining, u1_scheme, u1_auth, u1_token;
        std::string u2_remaining, u2_scheme, u2_auth, u2_token;
        split_scheme_auth_token(url1, u1_remaining, u1_scheme, u1_auth, u1_token);
        split_scheme_auth_token(url2, u2_remaining, u2_scheme, u2_auth, u2_token);
        return u1_remaining == u2_remaining;
    }

    bool is_path(const std::string& input)
    {
        static const std::regex re(R"(\./|\.\.|~|/|[a-zA-Z]:[/\\]|\\\\|//)");
        std::smatch sm;
        std::regex_search(input, sm, re);
        return !sm.empty() && sm.position(0) == 0 && input.find("://") == std::string::npos;
    }

    std::string path_to_url(const std::string& path)
    {
        static const std::string file_scheme = "file://";
        if (util::starts_with(path, file_scheme))
        {
            return path;
        }

        fs::u8path tmp_path = fs::u8path(path);
        std::string abs_path = fs::absolute(tmp_path).string();

        // TODO: handle percent encoding
        // https://blogs.msdn.microsoft.com/ie/2006/12/06/file-uris-in-windows/
        if (on_win)
        {
            util::replace_all(abs_path, "\\", "/");
        }
        return file_scheme + abs_path;
    }

    std::string file_uri_unc2_to_unc4(std::string_view uri)
    {
        static constexpr std::string_view file_scheme = "file:";

        // Not "file:" scheme
        if (!starts_with(uri, file_scheme))
        {
            return std::string(uri);
        }

        // No hostname set in "file://hostname/path/to/data.xml"
        auto [slashes, rest] = lstrip_parts(remove_prefix(uri, file_scheme), '/');
        if (slashes.size() != 2)
        {
            return std::string(uri);
        }

        const auto s_idx = rest.find('/');
        const auto c_idx = rest.find(':');

        // ':' found before '/', a Windows drive is specified in "file://C:/path/to/data.xml" (not
        // really URI compliant, they should have "file:///" or "file:/"). Otherwise no path in
        // "file://hostname", also not URI compliant.
        if (c_idx < s_idx)
        {
            return std::string(uri);
        }

        const auto hostname = rest.substr(0, s_idx);

        // '\' are used as path separator in "file://\\hostname\path\to\data.xml" (also not RFC
        // compliant)
        if (starts_with(hostname, R"(\\)"))
        {
            return std::string(uri);
        }

        // Things that means localhost are kept for some reason in ``url_to_path``
        // in ``conda.common.path``
        if ((hostname == "localhost") || (hostname == "127.0.0.1") || (hostname == "::1"))
        {
            return std::string(uri);
        }

        return concat("file:////", rest);
    }

    std::string encode_url(const std::string& url)
    {
        CURLEasyHandle curl = {};
        auto output = CURLStr(curl_easy_escape(curl.raw(), url.c_str(), static_cast<int>(url.size())));
        if (auto str = output.str())
        {
            return std::string(*str);
        }
        throw std::runtime_error("Could not url-escape string.");
    }

    std::string decode_url(const std::string& url)
    {
        CURLEasyHandle curl = {};
        int out_length;
        char* output = curl_easy_unescape(
            curl.raw(),
            url.c_str(),
            static_cast<int>(url.size()),
            &out_length
        );
        auto curl_str = CURLStr(output, out_length);
        if (auto str = curl_str.str())
        {
            return std::string(*str);
        }
        throw std::runtime_error("Could not url-unescape string.");
    }

    std::string cache_name_from_url(const std::string& url)
    {
        std::string u = url;
        if (u.empty() || (u.back() != '/' && !util::ends_with(u, ".json")))
        {
            u += '/';
        }

        // mimicking conda's behavior by special handling repodata.json
        // todo support .zst
        if (util::ends_with(u, "/repodata.json"))
        {
            u = u.substr(0, u.size() - 13);
        }

        unsigned char hash[16];

        EVP_MD_CTX* mdctx;
        mdctx = EVP_MD_CTX_create();
        EVP_DigestInit_ex(mdctx, EVP_md5(), nullptr);
        EVP_DigestUpdate(mdctx, u.c_str(), u.size());
        EVP_DigestFinal_ex(mdctx, hash, nullptr);
        EVP_MD_CTX_destroy(mdctx);

        std::string hex_digest = util::hex_string(hash, 16);
        return hex_digest.substr(0u, 8u);
    }

    /*****************************
     * URLHandler implementation *
     *****************************/

    auto URL::parse(std::string_view url, SchemeOpt opt) -> URL
    {
        auto out = URL();
        // CURL fails to parse the URL if no scheme is given, unless CURLU_DEFAULT_SCHEME
        // is given, but in this case, we loose the information about whether it was in the
        // original URL, so we parse it manually.
        if (opt == SchemeOpt::remove_if_present)
        {
        }
        else if (auto scheme = url_get_scheme(url); !scheme.empty())
        {
            out.set_scheme(scheme);
        }
        else if (opt == SchemeOpt::add_if_abscent)
        {
            out.set_scheme(URL::default_scheme);
        }

        const CURLUrl handle = {
            file_uri_unc2_to_unc4(url),
            CURLU_NON_SUPPORT_SCHEME | CURLU_DEFAULT_SCHEME,
        };
        out.set_user(handle.get_part(CURLUPART_USER).value_or(""))
            .set_password(handle.get_part(CURLUPART_PASSWORD).value_or(""))
            .set_host(handle.get_part(CURLUPART_HOST).value_or(""))
            .set_path(handle.get_part(CURLUPART_PATH).value_or(""))
            .set_port(handle.get_part(CURLUPART_PORT).value_or(""))
            .set_query(handle.get_part(CURLUPART_QUERY).value_or(""))
            .set_fragment(handle.get_part(CURLUPART_FRAGMENT).value_or(""));
        return out;
    }

    auto URL::str(SchemeOpt opt) const -> std::string
    {
        std::string_view const computed_scheme = [&]() -> std::string_view
        {
            if (opt == SchemeOpt::remove_if_present)
            {
                return "";
            }
            if (!m_scheme.empty())
            {
                return m_scheme;
            }
            else if (opt == SchemeOpt::add_if_abscent)
            {
                return URL::default_scheme;
            }
            return "";
        }();

        return concat(
            computed_scheme,
            computed_scheme.empty() ? "" : "://",
            m_user,
            m_password.empty() ? "" : ":",
            m_password,
            m_user.empty() ? "" : "@",
            m_host,
            m_port.empty() ? "" : ":",
            m_port,
            m_path,
            m_query.empty() ? "" : "?",
            m_query,
            m_fragment.empty() ? "" : "#",
            m_fragment
        );
    }

    auto URL::scheme() const -> const std::string&
    {
        return m_scheme;
    }

    auto URL::host() const -> const std::string&
    {
        return m_host;
    }

    auto URL::path() const -> const std::string&
    {
        return m_path;
    }

    auto URL::port() const -> const std::string&
    {
        return m_port;
    }

    auto URL::query() const -> const std::string&
    {
        return m_query;
    }

    auto URL::fragment() const -> const std::string&
    {
        return m_fragment;
    }

    auto URL::auth() const -> std::string
    {
        const auto& u = user();
        const auto& p = password();
        return (p != "") ? concat(u, ':', p) : u;
    }

    auto URL::user() const -> const std::string&
    {
        return m_user;
    }

    auto URL::password() const -> const std::string&
    {
        return m_password;
    }

    URL& URL::set_scheme(std::string_view scheme)
    {
        m_scheme = scheme;
        return *this;
    }

    URL& URL::set_host(std::string_view host)
    {
        m_host = host;
        return *this;
    }

    URL& URL::set_path(std::string_view path)
    {
        if (!starts_with(path, '/'))
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

    URL& URL::set_port(std::string_view port)
    {
        m_port = port;
        return *this;
    }

    URL& URL::set_query(std::string_view query)
    {
        m_query = query;
        return *this;
    }

    URL& URL::set_fragment(std::string_view fragment)
    {
        m_fragment = fragment;
        return *this;
    }

    URL& URL::set_user(std::string_view user)
    {
        m_user = user;
        return *this;
    }

    URL& URL::set_password(std::string_view password)
    {
        m_password = password;
        return *this;
    }
}  // namespace mamba
