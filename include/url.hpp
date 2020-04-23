#ifndef MAMBA_URL
#define MAMBA_URL

#include <string>
#include <stdexcept>

extern "C" {
    #include <curl/curl.h>
}

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

bool is_url(const std::string& url);

class URLParser
{
public:
    URLParser(const std::string& url);
    ~URLParser();

    std::string scheme();

    std::string m_url;
    CURLU* handle;
};

}

#endif