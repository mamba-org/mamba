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
    // bool has_scheme(const std::string& url)
    // {
    //     return std::regex_search(url, Context::instance().scheme_regex);
    // }

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
