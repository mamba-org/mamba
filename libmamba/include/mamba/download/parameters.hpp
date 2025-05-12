// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DOWNLOAD_PARAMETERS_HPP
#define MAMBA_DOWNLOAD_PARAMETERS_HPP

#include <functional>
#include <map>
#include <optional>
#include <string>

namespace mamba::download
{
    struct RemoteFetchParams
    {
        // ssl_verify can be either an empty string (regular SSL verification),
        // the string "<false>" to indicate no SSL verification, or a path to
        // a directory with cert files, or a cert file.
        std::string ssl_verify = "";
        bool ssl_no_revoke = false;
        bool curl_initialized = false;  // non configurable, used in fetch only

        std::string user_agent = "";

        double connect_timeout_secs = 10.;
        // int read_timeout_secs { 60 };
        int retry_timeout = 2;  // seconds
        int retry_backoff = 3;  // retry_timeout * retry_backoff
        int max_retries = 3;    // max number of retries

        std::map<std::string, std::string> proxy_servers;
    };

    struct Options
    {
        using termination_function = std::optional<std::function<void()>>;

        std::size_t download_threads = 1;
        bool fail_fast = false;
        bool sort = true;
        bool verbose = false;
        termination_function on_unexpected_termination = std::nullopt;
    };

}
#endif
