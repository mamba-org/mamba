// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DOWNLOAD_DOWNLOADER_HPP
#define MAMBA_DOWNLOAD_DOWNLOADER_HPP

#include <functional>
#include <optional>
#include <string>
#include <variant>

#include <tl/expected.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/download/mirror_map.hpp"
#include "mamba/download/request.hpp"

namespace mamba
{
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
        const mirror_map& mirrors,
        const Context& context,
        DownloadOptions options = {},
        DownloadMonitor* monitor = nullptr
    );

    DownloadResult download(
        DownloadRequest request,
        const mirror_map& mirrors,
        const Context& context,
        DownloadOptions options = {},
        DownloadMonitor* monitor = nullptr
    );

    bool check_resource_exists(const std::string& url, const Context& context);

}

#endif
