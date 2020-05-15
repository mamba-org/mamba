#include <iostream>
#include <regex>

#include "url.hpp"
#include "util.hpp"

namespace mamba
{
    bool has_scheme(const std::string& url)
    {
        std::regex re("[a-z][a-z0-9]{0,11}://");
        return std::regex_search(url, re);
    }

    void split_anaconda_token(const std::string& url,
                              std::string& cleaned_url,
                              std::string& token)
    {
        std::regex token_re("/t/([a-zA-Z0-9-]*)");
        auto token_begin = std::sregex_iterator(url.begin(), url.end(), token_re);
        if (token_begin != std::sregex_iterator())
        {
            token = token_begin->str().substr(3u);
            cleaned_url = std::regex_replace(url, token_re, "", std::regex_constants::format_first_only);
        }
        else
        {
            token = "";
            cleaned_url = url;
        }
        cleaned_url = rstrip(cleaned_url, "/");
    }

    void split_scheme_auth_token(const std::string& url,
                                 std::string& remaining_url,
                                 std::string& scheme,
                                 std::string& auth,
                                 std::string& token)
    {
        std::string cleaned_url;
        split_anaconda_token(url, cleaned_url, token);
        URLHandler handler(cleaned_url);
        scheme = handler.scheme();
        auth = handler.auth();
        handler.set_scheme("");
        handler.set_user("");
        handler.set_password("");
        remaining_url = handler.url();
    }

    void split_platform(const std::vector<std::string>& known_platforms,
                        const std::string& url,
                        std::string& cleaned_url,
                        std::string& platform)
    {
        platform = "";
        size_t pos = std::string::npos;
        for(auto it = known_platforms.begin(); it != known_platforms.end(); ++it)
        {
            pos = url.find(*it);
            if (pos != std::string::npos)
            {
                platform = *it;
                break;
            }
        }

        cleaned_url = url;
        if (pos != std::string::npos)
        {
            cleaned_url.replace(pos - 1, platform.size() + 1, ""); 
        }
        cleaned_url = rstrip(cleaned_url, "/");
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
        if (starts_with(path, file_scheme))
        {
            return path;
        }
        // TODO: handle percent encoding
        // https://blogs.msdn.microsoft.com/ie/2006/12/06/file-uris-in-windows/
        if (path.size() > 1u && path[1u] == ':')
        {
            return file_scheme + '/' + path;
        }
        else
        {
            return file_scheme + path;
        }
    }

    URLHandler::URLHandler(const std::string& url)
        : m_url(url)
        , m_has_scheme(has_scheme(url))
    {
        m_handle = curl_url(); /* get a handle to work with */ 
        if (!m_handle)
        {
            throw std::runtime_error("Could not initiate URL parser.");
        }
        if (m_has_scheme)
        {
            CURLUcode uc;
            uc = curl_url_set(m_handle, CURLUPART_URL, url.c_str(), CURLU_NON_SUPPORT_SCHEME);
            if (uc)
            {
                throw std::runtime_error("Could not set URL (code: " + std::to_string(uc) + ")");
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

    std::string URLHandler::url()
    {
        std::string res = get_part(CURLUPART_URL);
        if (!m_has_scheme)
        {
            res = res.substr(8);
        }
        return res;
    }

    std::string URLHandler::scheme()
    {
        return m_has_scheme ? get_part(CURLUPART_SCHEME) : "";
    }

    std::string URLHandler::host()
    {
        return get_part(CURLUPART_HOST);
    }

    std::string URLHandler::path()
    {
        return get_part(CURLUPART_PATH);
    }

    std::string URLHandler::port()
    {
        return get_part(CURLUPART_PORT);
    }

    std::string URLHandler::query()
    {
        return get_part(CURLUPART_QUERY);
    }

    std::string URLHandler::fragment()
    {
        return get_part(CURLUPART_FRAGMENT);
    }

    std::string URLHandler::options()
    {
        return get_part(CURLUPART_OPTIONS);
    }

    std::string URLHandler::auth()
    {
        std::string u = user();
        std::string p = password();
        return p != "" ? u + ':' + p : u;
    }
 
    std::string URLHandler::user()
    {
        return get_part(CURLUPART_USER);
    }

    std::string URLHandler::password()
    {
        return get_part(CURLUPART_PASSWORD);
    }

    std::string URLHandler::zoneid()
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
        const std::vector<std::string> CURLUPART_NAMES = 
        {
            "url",
            "scheme",
            "user",
            "password",
            "options",
            "host",
            "port",
            "path",
            "query",
            "fragment",
            "zoneid"
        };
    }

    std::string URLHandler::get_part(CURLUPart part)
    {
        char* scheme;
        auto rc = curl_url_get(m_handle, part, &scheme, m_has_scheme ? 0 : CURLU_DEFAULT_SCHEME);
        if (!rc)
        {
            std::string res(scheme);
            curl_free(scheme);
            return res;
        }
        else
        {
            throw std::runtime_error("Could not find " + CURLUPART_NAMES[part] + " of url " + m_url);
        }
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
} 
