#pragma once

extern "C" {
    #include <stdio.h>
    #include <curl/curl.h>
}

#if !CURL_AT_LEAST_VERSION(7, 62, 0)
#error "this example requires curl 7.62.0 or later"
#endif

// typedef enum {
//   CURLUE_OK,
//   CURLUE_BAD_HANDLE,          /* 1 */
//   CURLUE_BAD_PARTPOINTER,     /* 2 */
//   CURLUE_MALFORMED_INPUT,     /* 3 */
//   CURLUE_BAD_PORT_NUMBER,     /* 4 */
//   CURLUE_UNSUPPORTED_SCHEME,  /* 5 */
//   CURLUE_URLDECODE,           /* 6 */
//   CURLUE_OUT_OF_MEMORY,       /* 7 */
//   CURLUE_USER_NOT_ALLOWED,    /* 8 */
//   CURLUE_UNKNOWN_PART,        /* 9 */
//   CURLUE_NO_SCHEME,           /* 10 */
//   CURLUE_NO_USER,             /* 11 */
//   CURLUE_NO_PASSWORD,         /* 12 */
//   CURLUE_NO_OPTIONS,          /* 13 */
//   CURLUE_NO_HOST,             /* 14 */
//   CURLUE_NO_PORT,             /* 15 */
//   CURLUE_NO_QUERY,            /* 16 */
//   CURLUE_NO_FRAGMENT          /* 17 */
// } CURLUcode;

namespace mamba
{

class URLParser
{
public:
    URLParser(const std::string& url)
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

    std::string scheme()
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

    ~URLParser()
    {
        curl_url_cleanup(handle);
    }

    std::string m_url;
    CURLU* handle;
};

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
