#ifndef MAMBA_URL_HPP
#define MAMBA_URL_HPP

#include <string>
#include <stdexcept>

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

        void set_scheme(const std::string& scheme);
        void set_host(const std::string& host);
        void set_path(const std::string& path);
        void set_port(const std::string& port);

        void set_query(const std::string& query);
        void set_fragment(const std::string& fragment);
        void set_options(const std::string& options);

        void set_user(const std::string& user);
        void set_password(const std::string& password);
        void set_zoneid(const std::string& zoneid);

    private:

        std::string get_part(CURLUPart part);
        void set_part(CURLUPart part, const std::string& s);

        std::string m_url;
        CURLU* m_handle;
        bool m_scheme_set;
    };
}

#endif
