// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DL_DOWNLOADER_HPP
#define MAMBA_DL_DOWNLOADER_HPP

#include <functional>
#include <optional>
#include <string>
#include <variant>

#include <tl/expected.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/error_handling.hpp"

namespace mamba
{
    struct TransferData
    {
        int http_status = 0;
        std::string effective_url = "";
        std::size_t downloaded_size = 0;
        std::size_t average_speed_Bps = 0;
    };

    struct DownloadSuccess
    {
        std::string filename = "";
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

    struct DownloadProgress
    {
        std::size_t downloaded_size = 0;
        std::size_t total_to_download = 0;
        std::size_t speed_Bps = 0;
    };

    using DownloadEvent = std::variant<DownloadProgress, DownloadError, DownloadSuccess>;

    struct DownloadRequest
    {
        using progress_callback_t = std::function<void(const DownloadEvent&)>;

        // TODO: remove these functions when we plug a library with continuation
        using on_success_callback_t = std::function<expected_t<void>(const DownloadSuccess&)>;
        using on_failure_callback_t = std::function<void(const DownloadError&)>;

        std::string name;
        std::string url;
        std::string filename;
        bool head_only;
        bool ignore_failure;
        std::optional<std::size_t> expected_size = std::nullopt;
        std::optional<std::string> etag = std::nullopt;
        std::optional<std::string> last_modified = std::nullopt;

        std::optional<progress_callback_t> progress = std::nullopt;
        std::optional<on_success_callback_t> on_success = std::nullopt;
        std::optional<on_failure_callback_t> on_failure = std::nullopt;

        DownloadRequest(
            const std::string& lname,
            const std::string& lurl,
            const std::string& lfilename,
            bool lhead_only = false,
            bool lignore_failure = false
        );
    };

    using MultiDownloadRequest = std::vector<DownloadRequest>;

    using DownloadResult = tl::expected<DownloadSuccess, DownloadError>;
    using MultiDownloadResult = std::vector<DownloadResult>;

    struct DownloadOptions
    {
        using termination_function = std::optional<std::function<void()>>;

        bool fail_fast = false;
        bool sort = true;
        termination_function on_unexpected_termination = std::nullopt;
    };

    class DownloadMonitor
    {
    public:

        virtual ~DownloadMonitor() = default;

        DownloadMonitor(const DownloadMonitor&) = delete;
        DownloadMonitor& operator=(const DownloadMonitor&) = delete;
        DownloadMonitor(DownloadMonitor&&) = delete;
        DownloadMonitor& operator=(DownloadMonitor&&) = delete;

        void observe(MultiDownloadRequest& requests, DownloadOptions& options);
        void on_done();
        void on_unexpected_termination();

    protected:

        DownloadMonitor() = default;

    private:

        virtual void observe_impl(MultiDownloadRequest& requests, DownloadOptions& options) = 0;
        virtual void on_done_impl() = 0;
        virtual void on_unexpected_termination_impl() = 0;
    };

    MultiDownloadResult download(
        MultiDownloadRequest requests,
        const Context& context,
        DownloadOptions options = {},
        DownloadMonitor* monitor = nullptr
    );

    DownloadResult download(
        DownloadRequest request,
        const Context& context,
        DownloadOptions options = {},
        DownloadMonitor* monitor = nullptr
    );

    bool check_resource_exists(const std::string& url, const Context& context);

}

#endif
