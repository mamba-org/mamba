#include "mamba/core/download_progress_bar.hpp"

#include "progress_bar_impl.hpp"

namespace mamba
{
    /*******************************
     * ProgressBar for downloading *
     *******************************/

    namespace
    {
        using time_point = std::chrono::steady_clock::time_point;

        void update_progress_bar(
            ProgressProxy& progress_bar,
            time_point& throttle_time,
            const download::Progress& progress
        )
        {
            const auto now = std::chrono::steady_clock::now();

            const auto throttle_treshold = std::chrono::milliseconds(50);
            if (now - throttle_time < throttle_treshold)
            {
                return;
            }
            else
            {
                throttle_time = now;
            }

            if (!progress.total_to_download)
            {
                progress_bar.activate_spinner();
            }
            else
            {
                progress_bar.deactivate_spinner();
            }

            progress_bar.update_progress(progress.downloaded_size, progress.total_to_download);
            progress_bar.set_speed(progress.speed_Bps);
        }

        void update_progress_bar(ProgressProxy& progress_bar, const download::Error& error)
        {
            if (error.transfer.has_value())
            {
                const int http_status = error.transfer.value().http_status;
                progress_bar.set_postfix(std::to_string(http_status) + " failed");
            }
            else
            {
                progress_bar.set_postfix("failed");
            }
            progress_bar.set_full();
            progress_bar.mark_as_completed();
        }

        void update_progress_bar(ProgressProxy& progress_bar, const download::Success& success)
        {
            if (success.transfer.http_status == 304)
            {
                auto& r = progress_bar.repr();
                r.postfix.set_format("{:>20}", 20);
                r.prefix.set_format("{:<50}", 50);

                progress_bar.set_postfix("No change");
                progress_bar.mark_as_completed();

                r.total.deactivate();
                r.speed.deactivate();
                r.elapsed.deactivate();
            }
            else
            {
                progress_bar.repr().postfix.set_value("Downloaded").deactivate();
                progress_bar.mark_as_completed();

                // make sure total value is up-to-date
                progress_bar.update_repr(false);
                // select field to display and make sure they are
                // properly set if not yet printed by the progress bar manager
                ProgressBarRepr r = progress_bar.repr();
                r.prefix.set_format("{:<50}", 50);
                r.progress.deactivate();
                r.current.deactivate();
                r.separator.deactivate();

                auto console_stream = Console::stream();
                r.print(console_stream, 0, false);
            }
        }

        std::function<void(ProgressBarRepr&)> download_repr()
        {
            return [](ProgressBarRepr& r)
            {
                r.current.set_value(fmt::format(
                    "{:>7}",
                    to_human_readable_filesize(static_cast<double>(r.progress_bar().current()), 1)
                ));

                std::string total_str;
                if (!r.progress_bar().total()
                    || (r.progress_bar().total() == std::numeric_limits<std::size_t>::max()))
                {
                    total_str = "??.?MB";
                }
                else
                {
                    total_str = to_human_readable_filesize(
                        static_cast<double>(r.progress_bar().total()),
                        1
                    );
                }
                r.total.set_value(fmt::format("{:>7}", total_str));

                auto speed = r.progress_bar().speed();
                r.speed.set_value(fmt::format(
                    "@ {:>7}/s",
                    speed ? to_human_readable_filesize(static_cast<double>(speed), 1) : "??.?MB"
                ));

                r.separator.set_value("/");
            };
        }
    }

    /*********************
     * SubdirDataMonitor *
     *********************/

    SubdirDataMonitor::SubdirDataMonitor(MonitorOptions options)
        : m_options(std::move(options))
    {
    }

    void SubdirDataMonitor::reset_options(MonitorOptions options)
    {
        m_options = std::move(options);
    }

    bool SubdirDataMonitor::can_monitor(const Context& context)
    {
        return !(
            context.graphics_params.no_progress_bars || context.output_params.json
            || context.output_params.quiet
        );
    }

    void SubdirDataMonitor::observe_impl(download::MultiRequest& requests, download::Options& options)
    {
        m_throttle_time.resize(requests.size(), std::chrono::steady_clock::now());
        m_progress_bar.reserve(requests.size());
        for (std::size_t i = 0; i < requests.size(); ++i)
        {
            m_progress_bar.push_back(Console::instance().add_progress_bar(requests[i].name));
            m_progress_bar.back().set_repr_hook(download_repr());

            if (m_options.checking_download)
            {
                m_progress_bar.back().repr().postfix.set_value("Checking");
            }
            requests[i].progress = [this, i](const download::Event& e) { update_progress_bar(i, e); };
        }

        auto& pbar_manager = Console::instance().progress_bar_manager();
        if (!pbar_manager.started())
        {
            pbar_manager.watch_print();
        }

        options.on_unexpected_termination = [this]() { on_unexpected_termination(); };
    }

    void SubdirDataMonitor::on_done_impl()
    {
        auto& pbar_manager = Console::instance().progress_bar_manager();
        if (pbar_manager.started())
        {
            pbar_manager.terminate();
            if (!m_options.no_clear_progress_bar)
            {
                pbar_manager.clear_progress_bars();
            }
        }
        m_throttle_time.clear();
        m_progress_bar.clear();
        m_options = MonitorOptions{};
    }

    void SubdirDataMonitor::on_unexpected_termination_impl()
    {
        Console::instance().progress_bar_manager().terminate();
    }

    void SubdirDataMonitor::update_progress_bar(std::size_t index, const download::Event& event)
    {
        std::visit([this, index](auto&& arg) { update_progress_bar(index, arg); }, event);
    }

    void SubdirDataMonitor::update_progress_bar(std::size_t index, const download::Progress& progress)
    {
        mamba::update_progress_bar(m_progress_bar[index], m_throttle_time[index], progress);
    }

    void SubdirDataMonitor::update_progress_bar(std::size_t index, const download::Error& error)
    {
        if (m_options.checking_download)
        {
            complete_checking_progress_bar(index);
        }
        else
        {
            mamba::update_progress_bar(m_progress_bar[index], error);
        }
    }

    void SubdirDataMonitor::update_progress_bar(std::size_t index, const download::Success& success)
    {
        if (m_options.checking_download)
        {
            complete_checking_progress_bar(index);
        }
        else
        {
            mamba::update_progress_bar(m_progress_bar[index], success);
        }
    }

    void SubdirDataMonitor::complete_checking_progress_bar(std::size_t index)
    {
        ProgressProxy& progress_bar = m_progress_bar[index];
        progress_bar.repr().postfix.set_value("Checked");
        progress_bar.repr().speed.deactivate();
        progress_bar.repr().total.deactivate();
        progress_bar.mark_as_completed();
    }

    /**************************
     * PackageDownloadMonitor *
     **************************/

    bool PackageDownloadMonitor::can_monitor(const Context& context)
    {
        return SubdirDataMonitor::can_monitor(context);
    }

    PackageDownloadMonitor::~PackageDownloadMonitor()
    {
        end_monitoring();
    }

    void PackageDownloadMonitor::observe(
        download::MultiRequest& dl_requests,
        std::vector<PackageExtractTask>& extract_tasks,
        download::Options& options
    )
    {
        assert(extract_tasks.size() >= dl_requests.size());
        auto& pbar_manager = Console::instance().init_progress_bar_manager(ProgressBarMode::aggregated
        );
        m_extract_bar.reserve(extract_tasks.size());
        m_throttle_time.resize(dl_requests.size(), std::chrono::steady_clock::now());
        m_download_bar.reserve(dl_requests.size());

        for (size_t i = 0; i < extract_tasks.size(); ++i)
        {
            m_extract_bar.push_back(Console::instance().add_progress_bar(extract_tasks[i].name(), 1));
            init_extract_bar(m_extract_bar.back());
            extract_tasks[i].set_progress_callback([=](PackageExtractEvent e)
                                                   { update_extract_bar(i, e); });

            if (i < dl_requests.size())
            {
                assert(extract_tasks[i].needs_download());
                m_download_bar.push_back(Console::instance().add_progress_bar(dl_requests[i].name));
                init_download_bar(m_download_bar.back());
                dl_requests[i].progress = [this, i](const download::Event& e)
                { update_progress_bar(i, e); };
            }
        }

        init_aggregated_download();
        init_aggregated_extract();

        pbar_manager.start();
        pbar_manager.watch_print();

        options.on_unexpected_termination = [this]() { on_unexpected_termination(); };
    }

    void PackageDownloadMonitor::end_monitoring()
    {
        auto& pbar_manager = Console::instance().progress_bar_manager();
        if (pbar_manager.started())
        {
            pbar_manager.terminate();
        }
        m_throttle_time.clear();
        m_download_bar.clear();
        m_extract_bar.clear();
    }

    void PackageDownloadMonitor::init_extract_bar(ProgressProxy& extract_bar)
    {
        extract_bar.activate_spinner();
        extract_bar.set_progress_hook(
            [](ProgressProxy& bar) -> void
            {
                if (bar.started())
                {
                    bar.set_progress(0, 1);
                }
            }
        );
        extract_bar.set_repr_hook(
            [](ProgressBarRepr& r) -> void
            {
                if (r.progress_bar().started())
                {
                    r.postfix.set_value("Extracting");
                }
                else
                {
                    r.postfix.set_value("Extracted");
                }
            }
        );
        Console::instance().progress_bar_manager().add_label("Extract", extract_bar);
    }

    void PackageDownloadMonitor::init_download_bar(ProgressProxy& download_bar)
    {
        download_bar.set_repr_hook(download_repr());
        Console::instance().progress_bar_manager().add_label("Download", download_bar);
    }

    void PackageDownloadMonitor::init_aggregated_extract()
    {
        auto& pbar_manager = static_cast<AggregatedBarManager&>(
            Console::instance().progress_bar_manager()
        );
        auto* extract_bar = pbar_manager.aggregated_bar("Extract");
        if (extract_bar)
        {
            // lambda capture required because we are calling non const methods
            // on extract_bar.
            extract_bar->set_repr_hook(
                [extract_bar](ProgressBarRepr& repr) -> void
                {
                    auto active_tasks = extract_bar->active_tasks().size();
                    if (active_tasks == 0)
                    {
                        repr.prefix.set_value(fmt::format("{:<16}", "Extracting"));
                        repr.postfix.set_value(fmt::format("{:<25}", ""));
                    }
                    else
                    {
                        repr.prefix.set_value(
                            fmt::format("{:<11} {:>4}", "Extracting", fmt::format("({})", active_tasks))
                        );
                        repr.postfix.set_value(fmt::format("{:<25}", extract_bar->last_active_task()));
                    }
                    repr.current.set_value(fmt::format("{:>3}", extract_bar->current()));
                    repr.separator.set_value("/");

                    std::string total_str;
                    if (extract_bar->total() == std::numeric_limits<std::size_t>::max())
                    {
                        total_str = "?";
                    }
                    else
                    {
                        total_str = std::to_string(extract_bar->total());
                    }
                    repr.total.set_value(fmt::format("{:>3}", total_str));
                }
            );
        }
    }

    void PackageDownloadMonitor::init_aggregated_download()
    {
        auto& pbar_manager = static_cast<AggregatedBarManager&>(
            Console::instance().progress_bar_manager()
        );
        auto* dl_bar = pbar_manager.aggregated_bar("Download");
        if (dl_bar)
        {
            // lambda capture required because we are calling non const methods
            // on dl_bar.
            dl_bar->set_repr_hook(
                [dl_bar](ProgressBarRepr& repr) -> void
                {
                    auto active_tasks = dl_bar->active_tasks().size();
                    if (active_tasks == 0)
                    {
                        repr.prefix.set_value(fmt::format("{:<16}", "Downloading"));
                        repr.postfix.set_value(fmt::format("{:<25}", ""));
                    }
                    else
                    {
                        repr.prefix.set_value(
                            fmt::format("{:<11} {:>4}", "Downloading", fmt::format("({})", active_tasks))
                        );
                        repr.postfix.set_value(fmt::format("{:<25}", dl_bar->last_active_task()));
                    }
                    repr.current.set_value(
                        fmt::format("{:>7}", to_human_readable_filesize(double(dl_bar->current()), 1))
                    );
                    repr.separator.set_value("/");

                    std::string total_str;
                    if (dl_bar->total() == std::numeric_limits<std::size_t>::max())
                    {
                        total_str = "??.?MB";
                    }
                    else
                    {
                        total_str = to_human_readable_filesize(double(dl_bar->total()), 1);
                    }
                    repr.total.set_value(fmt::format("{:>7}", total_str));

                    auto speed = dl_bar->avg_speed(std::chrono::milliseconds(500));
                    repr.speed.set_value(
                        speed ? fmt::format("@ {:>7}/s", to_human_readable_filesize(double(speed), 1))
                              : ""
                    );
                }
            );
        }
    }

    void PackageDownloadMonitor::update_extract_bar(std::size_t index, PackageExtractEvent event)
    {
        switch (event)
        {
            case PackageExtractEvent::validate_update:
                m_extract_bar[index].set_postfix("validating");
                break;
            case PackageExtractEvent::validate_success:
                m_extract_bar[index].set_postfix("validated");
                break;
            case PackageExtractEvent::validate_failure:
                m_extract_bar[index].set_postfix("validation failed");
                break;
            case PackageExtractEvent::extract_update:
                m_extract_bar[index].update_progress(0, 1);
                break;
            case PackageExtractEvent::extract_success:
                m_extract_bar[index].set_full();
                m_extract_bar[index].mark_as_completed();
                break;
            case PackageExtractEvent::extract_failure:
            default:
                m_extract_bar[index].set_postfix("extraction failed");
                m_extract_bar[index].mark_as_completed();
                break;
        }
    }

    void PackageDownloadMonitor::observe_impl(download::MultiRequest&, download::Options&)
    {
        // Nothing to do, everything has been initialized in the public observe overload
    }

    void PackageDownloadMonitor::on_done_impl()
    {
        // Nothing to do, everything will be done in end_monitoring
    }

    void PackageDownloadMonitor::on_unexpected_termination_impl()
    {
        Console::instance().progress_bar_manager().terminate();
    }

    void PackageDownloadMonitor::update_progress_bar(std::size_t index, const download::Event& event)
    {
        std::visit([this, index](auto&& arg) { update_progress_bar(index, arg); }, event);
    }

    void
    PackageDownloadMonitor::update_progress_bar(std::size_t index, const download::Progress& progress)
    {
        mamba::update_progress_bar(m_download_bar[index], m_throttle_time[index], progress);
    }

    void PackageDownloadMonitor::update_progress_bar(std::size_t index, const download::Error& error)
    {
        mamba::update_progress_bar(m_download_bar[index], error);
    }

    void
    PackageDownloadMonitor::update_progress_bar(std::size_t index, const download::Success& success)
    {
        mamba::update_progress_bar(m_download_bar[index], success);
    }
}
