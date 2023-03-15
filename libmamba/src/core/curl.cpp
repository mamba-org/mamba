// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <spdlog/spdlog.h>

#include "curl.hpp"

namespace mamba
{
    const std::pair<std::string_view, CurlLogLevel> init_curl_ssl_session()
    {
        CURL* handle = curl_easy_init();
        if (handle)
        {
            std::pair<std::string_view, CurlLogLevel> log;
            const struct curl_tlssessioninfo* info = NULL;
            CURLcode res = curl_easy_getinfo(handle, CURLINFO_TLS_SSL_PTR, &info);
            if (info && !res)
            {
                if (info->backend == CURLSSLBACKEND_OPENSSL)
                {
                    log = std::make_pair(std::string_view("Using OpenSSL backend"), CurlLogLevel::kInfo);
                }
                else if (info->backend == CURLSSLBACKEND_SECURETRANSPORT)
                {
                    log = std::make_pair(
                        std::string_view("Using macOS SecureTransport backend"),
                        CurlLogLevel::kInfo
                    );
                }
                else if (info->backend == CURLSSLBACKEND_SCHANNEL)
                {
                    log = std::make_pair(
                        std::string_view("Using Windows Schannel backend"),
                        CurlLogLevel::kInfo
                    );
                }
                else if (info->backend != CURLSSLBACKEND_NONE)
                {
                    log = std::make_pair(
                        std::string_view("Using an unknown (to mamba) SSL backend"),
                        CurlLogLevel::kInfo
                    );
                }
                else if (info->backend == CURLSSLBACKEND_NONE)
                {
                    log = std::make_pair(
                        std::string_view(
                            "No SSL backend found! Please check how your cURL library is configured."
                        ),
                        CurlLogLevel::kWarning
                    );
                }
            }
            curl_easy_cleanup(handle);
            return log;
        }
        return std::make_pair(
            std::string_view("CURL handle was not properly intialized"),
            CurlLogLevel::kError
        );
    }

}  // namespace mamba
