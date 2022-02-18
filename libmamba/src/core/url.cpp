// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include "openssl/evp.h"

#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/context.hpp"

namespace mamba
{

    bool has_scheme(const std::string& url)
    {
        return std::regex_search(url, Context::instance().scheme_regex);
    }

    void split_anaconda_token(const std::string& url, std::string& cleaned_url, std::string& token)
    {
        auto token_begin
            = std::sregex_iterator(url.begin(), url.end(), Context::instance().token_regex);
        if (token_begin != std::sregex_iterator())
        {
            token = token_begin->str().substr(3u);
            cleaned_url = std::regex_replace(
                url, Context::instance().token_regex, "", std::regex_constants::format_first_only);
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
        remaining_url = rstrip(handler.url(), "/");
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
        if (starts_with(path, file_scheme))
        {
            return path;
        }

        fs::u8path tmp_path = fs::u8path(path);
        std::string abs_path = fs::absolute(tmp_path).string();

        // TODO: handle percent encoding
        // https://blogs.msdn.microsoft.com/ie/2006/12/06/file-uris-in-windows/
        if (on_win)
        {
            replace_all(abs_path, "\\", "/");
        }
        return file_scheme + abs_path;
    }

    std::string unc_url(const std::string& url)
    {
        // Replicate UNC behaviour of url_to_path from conda.common.path
        // We cannot use URLHandler for this since CURL returns an error when asked to parse
        // a url of type file://hostname/path
        // Colon character is excluded to make sure we do not match file URLs with absoulute
        // paths to a windows drive.
        static const std::regex file_host(R"(file://([^:/]*)(/.*)?)");
        std::smatch match;
        if (std::regex_match(url, match, file_host))
        {
            if (match[1] != "" && match[1] != "localhost" && match[1] != "127.0.0.1"
                && match[1] != "::1" && !starts_with(match[1].str(), R"(\\))"))
            {
                return "file:////" + std::string(match[1].first, url.cend());
            }
        }
        return url;
    }

    std::string encode_url(const std::string& url)
    {
        CURL* curl = curl_easy_init();
        if (curl)
        {
            char* output = curl_easy_escape(curl, url.c_str(), url.size());
            if (output)
            {
                std::string result(output);
                curl_free(output);
                curl_easy_cleanup(curl);
                return result;
            }
        }
        throw std::runtime_error("Could not url-escape string.");
    }

    std::string decode_url(const std::string& url)
    {
        CURL* curl = curl_easy_init();
        if (curl)
        {
            int out_length;
            char* output = curl_easy_unescape(curl, url.c_str(), url.size(), &out_length);
            if (output)
            {
                std::string result(output, out_length);
                curl_free(output);
                curl_easy_cleanup(curl);
                return result;
            }
        }
        throw std::runtime_error("Could not url-unescape string.");
    }

    std::string cache_name_from_url(const std::string& url)
    {
        std::string u = url;
        if (u.empty() || (u.back() != '/' && !ends_with(u, ".json")))
        {
            u += '/';
        }

        // mimicking conda's behavior by special handling repodata.json
        if (ends_with(u, "/repodata.json"))
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

        std::string hex_digest = hex_string(hash, 16);
        return hex_digest.substr(0u, 8u);
    }

    std::string hide_secrets(const std::string_view& str)
    {
        std::string copy(str);

        if (contains(str, "/t/"))
        {
            copy = std::regex_replace(copy, Context::instance().token_regex, "/t/*****");
        }

        copy = std::regex_replace(copy, Context::instance().http_basicauth_regex, "$1$2:*****@");

        return copy;
    }
}  // namespace mamba
