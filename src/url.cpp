#include "url.hpp"

namespace mamba
{
    bool is_url(const std::string& url)
    {
        if (url.size() == 0) return false;
        try
        {
            URLParser p(url); 
            return p.scheme().size() != 0;
        }
        catch(...)
        {
            return false;
        }
    }

    URLParser::URLParser(const std::string& url)
        : m_url(url)
    {
        m_handle = curl_url(); /* get a handle to work with */ 
        if (!m_handle)
        {
            throw std::runtime_error("Could not initiate URL parser.");
        }
        CURLUcode uc;
        uc = curl_url_set(m_handle, CURLUPART_URL, url.c_str(), CURLU_NON_SUPPORT_SCHEME);
        if (uc)
        {
            throw std::runtime_error("Could not set URL (code: " + std::to_string(uc) + ")");
        }
    }

    URLParser::~URLParser()
    {
        curl_url_cleanup(m_handle);
    }

    std::string URLParser::url()
    {
        return get_part(CURLUPART_URL);
    }

    std::string URLParser::scheme()
    {
        return get_part(CURLUPART_SCHEME);
    }

    std::string URLParser::host()
    {
        return get_part(CURLUPART_HOST);
    }

    std::string URLParser::path()
    {
        return get_part(CURLUPART_PATH);
    }

    std::string URLParser::port()
    {
        return get_part(CURLUPART_PORT);
    }

    std::string URLParser::query()
    {
        return get_part(CURLUPART_QUERY);
    }

    std::string URLParser::fragment()
    {
        return get_part(CURLUPART_FRAGMENT);
    }

    std::string URLParser::options()
    {
        return get_part(CURLUPART_OPTIONS);
    }

    std::string URLParser::user()
    {
        return get_part(CURLUPART_USER);
    }

    std::string URLParser::password()
    {
        return get_part(CURLUPART_PASSWORD);
    }

    std::string URLParser::zoneid()
    {
        return get_part(CURLUPART_ZONEID);
    }


    std::string URLParser::get_part(CURLUPart part)
    {
        char* scheme;
        auto rc = curl_url_get(m_handle, part, &scheme, 0);
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
} 
