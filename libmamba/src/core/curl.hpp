// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CURL_HPP
#define MAMBA_CURL_HPP

#include <stdexcept>
#include <string_view>
#include <utility>

extern "C"
{
#include <curl/curl.h>
}

namespace mamba
{
    enum class CurlLogLevel
    {
        kInfo,
        kWarning,
        kError
    };

    const std::pair<std::string_view, CurlLogLevel> init_curl_ssl_session();

}  // namespace mamba

#endif  // MAMBA_CURL_HPP
