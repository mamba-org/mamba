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
#include "mamba/core/package_fetcher.hpp"

namespace mamba
{
    struct MonitorOptions
    {
        bool checking_download = false;
        bool no_clear_progress_bar = false;
    };

    class SubdirDataMonitor : public DownloadMonitor
    {
    public:

        static bool can_monitor(const Context& context);

        explicit SubdirDataMonitor(MonitorOptions options = {});
        virtual ~SubdirDataMonitor() = default;

        SubdirDataMonitor(const SubdirDataMonitor&) = delete;
        SubdirDataMonitor& operator=(const SubdirDataMonitor&) = delete;

        SubdirDataMonitor(SubdirDataMonitor&&) = delete;
        SubdirDataMonitor& operator=(SubdirDataMonitor&&) = delete;

        void reset_options(MonitorOptions options);

    private:

        void observe_impl(MultiDownloadRequest& requests, DownloadOptions& options) override;
        void on_done_impl() override;
        void on_unexpected_termination_impl() override;

        void update_progress_bar(std::size_t index, const DownloadEvent& event);
        void update_progress_bar(std::size_t index, const DownloadProgress& progress);
        void update_progress_bar(std::size_t index, const DownloadError& error);
        void update_progress_bar(std::size_t index, const DownloadSuccess& success);

        void complete_checking_progress_bar(std::size_t index);

        using time_point = std::chrono::steady_clock::time_point;
        std::vector<time_point> m_throttle_time;
        std::vector<ProgressProxy> m_progress_bar;
        MonitorOptions m_options;
    };

    class PackageDownloadMonitor : public DownloadMonitor
    {
    public:

        static bool can_monitor(const Context& context);

        PackageDownloadMonitor() = default;
        virtual ~PackageDownloadMonitor();

        PackageDownloadMonitor(const PackageDownloadMonitor&) = delete;
        PackageDownloadMonitor& operator=(const PackageDownloadMonitor&) = delete;

        PackageDownloadMonitor(PackageDownloadMonitor&&) = delete;
        PackageDownloadMonitor& operator=(PackageDownloadMonitor&&) = delete;

        // Requires extract_tasks.size() >= requests.size()
        // Requires for i in [0, dl_requests.size()), extract_tasks[i].needs_download()
        void observe(
            MultiDownloadRequest& dl_requests,
            std::vector<PackageExtractRequest>& extract_requests,
            DownloadOptions& options
        );

        void end_monitoring();

    private:

        void init_extract_bar(ProgressProxy& extract_bar);
        void init_download_bar(ProgressProxy& download_bar);
        void init_aggregated_extract();
        void init_aggregated_download();

        void update_extract_bar(std::size_t index, PackageExtractEvent event);

        void observe_impl(MultiDownloadRequest& requests, DownloadOptions& options) override;
        void on_done_impl() override;
        void on_unexpected_termination_impl() override;

        void update_progress_bar(std::size_t index, const DownloadEvent& event);
        void update_progress_bar(std::size_t index, const DownloadProgress& progress);
        void update_progress_bar(std::size_t index, const DownloadError& error);
        void update_progress_bar(std::size_t index, const DownloadSuccess& success);

        std::vector<ProgressProxy> m_extract_bar;

        using time_point = std::chrono::steady_clock::time_point;
        std::vector<time_point> m_throttle_time;
        std::vector<ProgressProxy> m_download_bar;
    };
}

#endif
