// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DOWNLOAD_REQUEST_HPP
#define MAMBA_DOWNLOAD_REQUEST_HPP

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "mamba/core/error_handling.hpp"

namespace mamba
{
    /*******************************
     * Download results structures *
     *******************************/

    struct TransferData
    {
        int http_status = 0;
        std::string effective_url = "";
        std::size_t downloaded_size = 0;
        std::size_t average_speed_Bps = 0;
    };

    struct DownloadFilename
    {
        std::string value = "";
    };

    struct DownloadBuffer
    {
        std::string value = "";
    };

    using DownloadContent = std::variant<DownloadFilename, DownloadBuffer>;

    struct DownloadSuccess
    {
        DownloadContent content = {};
        TransferData transfer = {};
        std::string cache_control = "";
        std::string etag = "";
        std::string last_modified = "";
        std::size_t attempt_number = std::size_t(1);
    };

    struct DownloadError
    {
        std::string message = "";
        std::optional<std::size_t> retry_wait_seconds = std::nullopt;
        std::optional<TransferData> transfer = std::nullopt;
        std::size_t attempt_number = std::size_t(1);
    };

    using DownloadResult = tl::expected<DownloadSuccess, DownloadError>;
    using MultiDownloadResult = std::vector<DownloadResult>;

    /*****************************
     * Download event structures *
     *****************************/

    struct DownloadProgress
    {
        std::size_t downloaded_size = 0;
        std::size_t total_to_download = 0;
        std::size_t speed_Bps = 0;
    };

    using DownloadEvent = std::variant<DownloadProgress, DownloadError, DownloadSuccess>;

    /*******************************
     * Download request structures *
     *******************************/

    struct DownloadRequestBase
    {
        using progress_callback_t = std::function<void(const DownloadEvent&)>;

        // TODO: remove these functions when we plug a library with continuation
        using on_success_callback_t = std::function<expected_t<void>(const DownloadSuccess&)>;
        using on_failure_callback_t = std::function<void(const DownloadError&)>;

        std::string name;
        // If filename is not initialized, the data will be downloaded
        // to an internal in-memory buffer.
        std::optional<std::string> filename;
        bool head_only;
        bool ignore_failure;
        std::optional<std::size_t> expected_size = std::nullopt;
        std::optional<std::string> etag = std::nullopt;
        std::optional<std::string> last_modified = std::nullopt;

        std::optional<progress_callback_t> progress = std::nullopt;
        std::optional<on_success_callback_t> on_success = std::nullopt;
        std::optional<on_failure_callback_t> on_failure = std::nullopt;

    protected:

        DownloadRequestBase(
            std::string_view lname,
            std::optional<std::string> lfilename,
            bool lhead_only,
            bool lignore_failure
        );

        ~DownloadRequestBase() = default;
        DownloadRequestBase(const DownloadRequestBase&) = default;
        DownloadRequestBase& operator=(const DownloadRequestBase&) = default;
        DownloadRequestBase(DownloadRequestBase&&) = default;
        DownloadRequestBase& operator=(DownloadRequestBase&&) = default;
    };

    struct DownloadRequest : DownloadRequestBase
    {
        std::string mirror_name;
        std::string url_path;

        DownloadRequest(
            std::string_view lname,
            std::string_view lmirror_name,
            std::string_view lurl_path,
            std::optional<std::string> lfilename = std::nullopt,
            bool lhead_only = false,
            bool lignore_failure = false
        );

        ~DownloadRequest() = default;
        DownloadRequest(const DownloadRequest&) = default;
        DownloadRequest& operator=(const DownloadRequest&) = default;
        DownloadRequest(DownloadRequest&&) = default;
        DownloadRequest& operator=(DownloadRequest&&) = default;
    };

    using MultiDownloadRequest = std::vector<DownloadRequest>;
}

#endif

