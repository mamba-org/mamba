#include <iostream>
#include <regex>

#include "url.hpp"
#include "util.hpp"

namespace mamba
{
    bool is_url(const std::string& url)
    {
        if (url.size() == 0) return false;
        try
        {
            URLHandler p(url); 
            return p.scheme().size() != 0;
        }
        catch(...)
        {
            return false;
        }
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

    URLHandler::URLHandler(const std::string& url)
        : m_url(url)
        , m_scheme_set(url != "")
    {
        m_handle = curl_url(); /* get a handle to work with */ 
        if (!m_handle)
        {
            throw std::runtime_error("Could not initiate URL parser.");
        }
        if (m_scheme_set)
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

    std::string URLHandler::url()
    {
        std::string res = get_part(CURLUPART_URL);
        if (!m_scheme_set)
        {
            res = res.substr(8);
        }
        return res;
    }

    std::string URLHandler::scheme()
    {
        return get_part(CURLUPART_SCHEME);
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

    void URLHandler::set_scheme(const std::string& scheme)
    {
        m_scheme_set = (scheme != "");
        if (m_scheme_set)
        {
            set_part(CURLUPART_SCHEME, scheme);
        }
    }

    void URLHandler::set_host(const std::string& host)
    {
        set_part(CURLUPART_HOST, host);
    }

    void URLHandler::set_path(const std::string& path)
    {
        set_part(CURLUPART_PATH, path);
    }

    void URLHandler::set_port(const std::string& port)
    {
        set_part(CURLUPART_PORT, port);
    }
    
    void URLHandler::set_query(const std::string& query)
    {
        set_part(CURLUPART_QUERY, query);
    }

    void URLHandler::set_fragment(const std::string& fragment)
    {
        set_part(CURLUPART_FRAGMENT, fragment);
    }

    void URLHandler::set_options(const std::string& options)
    {
        set_part(CURLUPART_OPTIONS, options);
    }

    void URLHandler::set_user(const std::string& user)
    {
        set_part(CURLUPART_USER, user);
    }

    void URLHandler::set_password(const std::string& password)
    {
        set_part(CURLUPART_PASSWORD, password);
    }

    void URLHandler::set_zoneid(const std::string& zoneid)
    {
        set_part(CURLUPART_ZONEID, zoneid);
    }
    
    std::string URLHandler::get_part(CURLUPart part)
    {
        char* scheme;
        auto rc = curl_url_get(m_handle, part, &scheme, m_scheme_set ? 0 : CURLU_DEFAULT_SCHEME);
        if (!rc)
        {
            std::string res(scheme);
            curl_free(scheme);
            return res;
        }
        else
        {
            throw std::runtime_error("Could not find SCHEME of url " + m_url);
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
