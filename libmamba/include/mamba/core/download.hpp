// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_DOWNLOAD_HPP
#define MAMBA_CORE_DOWNLOAD_HPP

#include <functional>
#include <optional>
#include <string>
#include <variant>

#include <tl/expected.hpp>

#include "mamba/core/context.hpp"

namespace mamba
{
    struct TransferData
    {
        int http_status;
        std::string effective_url;
        std::size_t downloaded_size;
        std::size_t average_speed;
    };

    struct DownloadSuccess
    {
        std::string filename;
        TransferData transfer;
        std::string cache_control;
        std::string etag;
        std::string last_modified;
    };

    struct DownloadError
    {
        std::string message;
        std::optional<std::size_t> retry_wait_seconds = std::nullopt;
        std::optional<TransferData> transfer = std::nullopt;
    };

    struct DownloadProgress
    {
        std::size_t downloaded_size;
        std::size_t total_to_download;
    };

    using DownloadEvent = std::variant<
        DownloadProgress,
        DownloadError,
        DownloadSuccess
    >;

    struct DownloadRequest
    {
        using progress_callback_t = std::function<void(const DownloadEvent&)>;

        // TODO: remove these functions when we plug a library with continuation
        using on_success_callback_t = std::function<bool(const DownloadSuccess&)>;
        using on_failure_callback_t = std::function<void(const DownloadError&)>;

        std::string name;
        std::string url;
        std::string filename;
        bool ignore_failure;
        std::optional<std::size_t> expected_size;
        std::optional<std::string> if_none_match;
        std::optional<std::string> if_modified_since;

        std::optional<progress_callback_t> progress;
        std::optional<on_success_callback_t> on_success;
        std::optional<on_failure_callback_t> on_failure;

        DownloadRequest(
            const std::string& lname,
            const std::string& lurl,
            const std::string& lfilename,
            bool lignore_failure = false
        );
    };

    using DownloadRequestList = std::vector<DownloadRequest>;

    struct MultiDownloadRequest
    {
        DownloadRequestList requests;
    };

    using DownloadResult = tl::expected<DownloadSuccess, DownloadError>;
    using DownloadResultList = std::vector<DownloadResult>;

    struct MultiDownloadResult
    {
        DownloadResultList results;
    };

    struct DownloadOptions
    {
        bool fail_fast;
        bool sort;
    };

    MultiDownloadResult download(MultiDownloadRequest requests,
                                 const Context& context,
                                 DownloadOptions options);
}

#endif

