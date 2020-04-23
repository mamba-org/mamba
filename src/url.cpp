#include "url.hpp"

namespace mamba
{

    URLParser::URLParser(const std::string& url)
        : m_url(url)
    {
        char* host;
        char* path;

        handle = curl_url(); /* get a handle to work with */ 
        if (!handle)
        {
            throw std::runtime_error("Could not initiate URL parser.");
        }
        CURLUcode uc;
        uc = curl_url_set(handle, CURLUPART_URL, url.c_str(), CURLU_NON_SUPPORT_SCHEME);
        if (uc)
        {
            throw std::runtime_error("Could not set URL (code: " + std::to_string(uc) + ")");
        }
    }

    URLParser::~URLParser()
    {
        curl_url_cleanup(handle);
    }

    std::string URLParser::scheme()
    {
        char* scheme;
        auto rc = curl_url_get(handle, CURLUPART_SCHEME, &scheme, 0);
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


	bool is_url(const std::string& url)
	{
	    if (url.size() == 0) return false;
	    try {
	        URLParser p(url);
	        if (p.scheme().size() != 0) {
	            return true;
	        }
	    }
	    catch(...)
	    {
	        return false;
	    }
	    return false;
	}

}