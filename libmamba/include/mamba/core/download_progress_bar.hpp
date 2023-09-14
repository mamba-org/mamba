// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_DOWNLOAD_PROGRESS_BAR_HPP
#define MAMBA_CORE_DOWNLOAD_PROGRESS_BAR_HPP

#include <chrono>
#include <functional>
#include <vector>

#include "mamba/core/context.hpp"
#include "mamba/core/download.hpp"

namespace mamba
{
    struct MonitorOptions
    {
        bool checking_download = false;
        bool no_clear_progress_bar = false;
    };

    class DownloadProgressBar : public DownloadMonitor
    {
    public:

        static bool can_monitor(const Context& context);

        explicit DownloadProgressBar(MonitorOptions options = {});
        virtual ~DownloadProgressBar() = default;

        DownloadProgressBar(const DownloadProgressBar&) = delete;
        DownloadProgressBar& operator=(const DownloadProgressBar&) = delete;

        DownloadProgressBar(DownloadProgressBar&&) = delete;
        DownloadProgressBar& operator=(DownloadProgressBar&&) = delete;

        void reset_options(MonitorOptions options);

        void observe(MultiDownloadRequest& requests, DownloadOptions& options) override;
        void on_done() override;
        void on_unexpected_termination() override;

    private:

        void update_progress_bar(size_t index, const DownloadEvent& event);
        void update_progress_bar(size_t index, const DownloadProgress& progress);
        void update_progress_bar(size_t index, const DownloadError& error);
        void update_progress_bar(size_t index, const DownloadSuccess& success);

        void complete_checking_progress_bar(size_t index);

        std::function<void(ProgressBarRepr&)> download_repr(size_t index);

        using time_point = std::chrono::steady_clock::time_point;
        std::vector<time_point> m_throttle_time;
        std::vector<ProgressProxy> m_progress_bar;
        MonitorOptions m_options;
    };

}

#endif
