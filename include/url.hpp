#ifndef MAMBA_URL_HPP
#define MAMBA_URL_HPP

#include <string>
#include <stdexcept>
#include <vector>

extern "C"
{
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

    void split_anaconda_token(const std::string& url,
                              std::string& cleaned_url,
                              std::string& token);
    
    void split_scheme_auth_token(const std::string& url,
                                 std::string& remaining_url,
                                 std::string& scheme,
                                 std::string& auth,
                                 std::string& token);

    void split_platform(const std::vector<std::string>& known_platforms,
                        const std::string& url,
                        std::string& cleaned_url,
                        std::string& platform);

    class URLHandler
    {
    public:
    
        URLHandler(const std::string& url = "");
        ~URLHandler();

        URLHandler(const URLHandler&);
        URLHandler& operator=(const URLHandler&);

        URLHandler(URLHandler&&);
        URLHandler& operator=(URLHandler&&);

        std::string url();

        std::string scheme();
        std::string host();
        std::string path();
        std::string port();

        std::string query();
        std::string fragment();
        std::string options();

        std::string auth();
        std::string user();
        std::string password();
        std::string zoneid();

        URLHandler& set_scheme(const std::string& scheme);
        URLHandler& set_host(const std::string& host);
        URLHandler& set_path(const std::string& path);
        URLHandler& set_port(const std::string& port);

        URLHandler& set_query(const std::string& query);
        URLHandler& set_fragment(const std::string& fragment);
        URLHandler& set_options(const std::string& options);

        URLHandler& set_user(const std::string& user);
        URLHandler& set_password(const std::string& password);
        URLHandler& set_zoneid(const std::string& zoneid);

    private:

        std::string get_part(CURLUPart part);
        void set_part(CURLUPart part, const std::string& s);

        std::string m_url;
        CURLU* m_handle;
        bool m_scheme_set;
    };
}

#endif
