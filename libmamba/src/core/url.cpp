// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>
#include <regex>
#include <string_view>

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

    bool has_scheme(const std::string& url)
    {
        return std::regex_search(url, Context::instance().scheme_regex);
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
        URLHandler handler(cleaned_url);
        scheme = handler.scheme();
        auth = handler.auth();
        handler.set_scheme("");
        handler.set_user("");
        handler.set_password("");
        remaining_url = util::rstrip(handler.url(), "/");
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

    URLHandler::URLHandler(const std::string& url)
        : m_url(url)
        , m_has_scheme(has_scheme(url))
    {
        m_handle = curl_url(); /* get a handle to work with */
        if (!m_handle)
        {
            throw std::runtime_error("Could not initiate URL parser.");
        }

        if (!url.empty())
        {
            CURLUcode uc;
            auto curl_flags = m_has_scheme ? CURLU_NON_SUPPORT_SCHEME : CURLU_DEFAULT_SCHEME;
            std::string c_url = file_uri_unc2_to_unc4(url);
            uc = curl_url_set(
                m_handle,
                CURLUPART_URL,
                c_url.c_str(),
                static_cast<unsigned int>(curl_flags)
            );
            if (uc)
            {
                throw std::runtime_error(
                    "Could not set URL (code: " + std::to_string(uc) + " - url = " + url + ")"
                );
            }
        }
    }

    URLHandler::~URLHandler()
    {
        curl_url_cleanup(m_handle);
    }

    URLHandler::URLHandler(const URLHandler& rhs)
        : m_url(rhs.m_url)
        , m_handle(curl_url_dup(rhs.m_handle))
        , m_has_scheme(rhs.m_has_scheme)
    {
    }

    URLHandler& URLHandler::operator=(const URLHandler& rhs)
    {
        URLHandler tmp(rhs);
        std::swap(tmp, *this);
        return *this;
    }

    URLHandler::URLHandler(URLHandler&& rhs)
        : m_url(std::move(rhs.m_url))
        , m_handle(rhs.m_handle)
        , m_has_scheme(std::move(rhs.m_has_scheme))
    {
        rhs.m_handle = nullptr;
    }

    URLHandler& URLHandler::operator=(URLHandler&& rhs)
    {
        std::swap(m_url, rhs.m_url);
        std::swap(m_handle, rhs.m_handle);
        std::swap(m_has_scheme, rhs.m_has_scheme);
        return *this;
    }

    std::string URLHandler::url(bool strip_scheme)
    {
        std::string res = get_part(CURLUPART_URL);
        if (!res.empty())
        {
            if (!m_has_scheme || strip_scheme)
            {
                // Default scheme is https:// OR file://
                auto pos = res.find("://");
                if (pos != std::string::npos)
                {
                    res = res.substr(pos + 3u);
                }
            }
        }
        return res;
    }

    std::string URLHandler::scheme() const
    {
        return m_has_scheme ? get_part(CURLUPART_SCHEME) : "";
    }

    std::string URLHandler::host() const
    {
        return get_part(CURLUPART_HOST);
    }

    std::string URLHandler::path() const
    {
        return get_part(CURLUPART_PATH);
    }

    std::string URLHandler::port() const
    {
        return get_part(CURLUPART_PORT);
    }

    std::string URLHandler::query() const
    {
        return get_part(CURLUPART_QUERY);
    }

    std::string URLHandler::fragment() const
    {
        return get_part(CURLUPART_FRAGMENT);
    }

    std::string URLHandler::options() const
    {
        return get_part(CURLUPART_OPTIONS);
    }

    std::string URLHandler::auth() const
    {
        std::string u = user();
        std::string p = password();
        return p != "" ? u + ':' + p : u;
    }

    std::string URLHandler::user() const
    {
        return get_part(CURLUPART_USER);
    }

    std::string URLHandler::password() const
    {
        return get_part(CURLUPART_PASSWORD);
    }

    std::string URLHandler::zoneid() const
    {
        return get_part(CURLUPART_ZONEID);
    }

    URLHandler& URLHandler::set_scheme(const std::string& scheme)
    {
        m_has_scheme = (scheme != "");
        if (m_has_scheme)
        {
            set_part(CURLUPART_SCHEME, scheme);
        }
        return *this;
    }

    URLHandler& URLHandler::set_host(const std::string& host)
    {
        set_part(CURLUPART_HOST, host);
        return *this;
    }

    URLHandler& URLHandler::set_path(const std::string& path)
    {
        set_part(CURLUPART_PATH, path);
        return *this;
    }

    URLHandler& URLHandler::set_port(const std::string& port)
    {
        set_part(CURLUPART_PORT, port);
        return *this;
    }

    URLHandler& URLHandler::set_query(const std::string& query)
    {
        set_part(CURLUPART_QUERY, query);
        return *this;
    }

    URLHandler& URLHandler::set_fragment(const std::string& fragment)
    {
        set_part(CURLUPART_FRAGMENT, fragment);
        return *this;
    }

    URLHandler& URLHandler::set_options(const std::string& options)
    {
        set_part(CURLUPART_OPTIONS, options);
        return *this;
    }

    URLHandler& URLHandler::set_user(const std::string& user)
    {
        set_part(CURLUPART_USER, user);
        return *this;
    }

    URLHandler& URLHandler::set_password(const std::string& password)
    {
        set_part(CURLUPART_PASSWORD, password);
        return *this;
    }

    URLHandler& URLHandler::set_zoneid(const std::string& zoneid)
    {
        set_part(CURLUPART_ZONEID, zoneid);
        return *this;
    }

    namespace
    {
        const std::vector<std::string> CURLUPART_NAMES = { "url",      "scheme",  "user",
                                                           "password", "options", "host",
                                                           "port",     "path",    "query",
                                                           "fragment", "zoneid" };
    }

    std::string URLHandler::get_part(CURLUPart part) const
    {
        CURLStr scheme{};
        auto rc = curl_url_get(m_handle, part, scheme.raw_input(), m_has_scheme ? 0 : CURLU_DEFAULT_SCHEME);
        if (!rc)
        {
            if (auto str = scheme.str())
            {
                return std::string(*str);
            }
        }
        return "";
    }

    void URLHandler::set_part(CURLUPart part, const std::string& s)
    {
        const char* data = s == "" ? nullptr : s.c_str();
        auto rc = curl_url_set(m_handle, part, data, CURLU_NON_SUPPORT_SCHEME);
        if (rc)
        {
            throw std::runtime_error("Could not set " + s + " in url " + m_url);
        }
    }
}  // namespace mamba
