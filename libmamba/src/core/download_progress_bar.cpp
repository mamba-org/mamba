#include "mamba/core/download_progress_bar.hpp"

#include "progress_bar_impl.hpp"

namespace mamba
{
    DownloadProgressBar::DownloadProgressBar(MonitorOptions options)
        : m_options(std::move(options))
    {
    }

    void DownloadProgressBar::reset_options(MonitorOptions options)
    {
        m_options = std::move(options);
    }

    bool DownloadProgressBar::can_monitor(const Context& context)
    {
        return !(
            context.graphics_params.no_progress_bars || context.output_params.json
            || context.output_params.quiet
        );
    }

    void DownloadProgressBar::observe_impl(MultiDownloadRequest& requests, DownloadOptions& options)
    {
        m_throttle_time.resize(requests.size(), std::chrono::steady_clock::now());
        m_progress_bar.reserve(requests.size());
        for (std::size_t i = 0; i < requests.size(); ++i)
        {
            m_progress_bar.push_back(Console::instance().add_progress_bar(requests[i].name));
            m_progress_bar.back().set_repr_hook(download_repr(i));

            if (m_options.checking_download)
            {
                m_progress_bar.back().repr().postfix.set_value("Checking");
            }
            requests[i].progress = [this, i](const DownloadEvent& e) { update_progress_bar(i, e); };
        }

        auto& pbar_manager = Console::instance().progress_bar_manager();
        if (!pbar_manager.started())
        {
            pbar_manager.watch_print();
        }

        options.on_unexpected_termination = [this]() { on_unexpected_termination(); };
    }

    void DownloadProgressBar::on_done_impl()
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

    void DownloadProgressBar::on_unexpected_termination_impl()
    {
        Console::instance().progress_bar_manager().terminate();
    }

    void DownloadProgressBar::update_progress_bar(std::size_t index, const DownloadEvent& event)
    {
        std::visit([this, index](auto&& arg) { update_progress_bar(index, arg); }, event);
    }

    void DownloadProgressBar::update_progress_bar(std::size_t index, const DownloadProgress& progress)
    {
        auto now = std::chrono::steady_clock::now();

        const auto throttle_treshold = std::chrono::milliseconds(50);
        if (now - m_throttle_time[index] < throttle_treshold)
        {
            return;
        }
        else
        {
            m_throttle_time[index] = now;
        }

        ProgressProxy& progress_bar = m_progress_bar[index];
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

    void DownloadProgressBar::update_progress_bar(std::size_t index, const DownloadError& error)
    {
        if (m_options.checking_download)
        {
            complete_checking_progress_bar(index);
        }
        else
        {
            ProgressProxy& progress_bar = m_progress_bar[index];
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
    }

    void DownloadProgressBar::update_progress_bar(std::size_t index, const DownloadSuccess& success)
    {
        if (m_options.checking_download)
        {
            complete_checking_progress_bar(index);
        }
        else
        {
            ProgressProxy& progress_bar = m_progress_bar[index];
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
    }

    void DownloadProgressBar::complete_checking_progress_bar(std::size_t index)
    {
        ProgressProxy& progress_bar = m_progress_bar[index];
        progress_bar.repr().postfix.set_value("Checked");
        progress_bar.repr().speed.deactivate();
        progress_bar.repr().total.deactivate();
        progress_bar.mark_as_completed();
    }

    std::function<void(ProgressBarRepr&)> DownloadProgressBar::download_repr(std::size_t index)
    {
        return [this, index](ProgressBarRepr& r)
        {
            ProgressProxy& progress_bar = m_progress_bar[index];
            r.current.set_value(fmt::format(
                "{:>7}",
                to_human_readable_filesize(static_cast<double>(progress_bar.current()), 1)
            ));

            std::string total_str;
            if (!progress_bar.total()
                || (progress_bar.total() == std::numeric_limits<std::size_t>::max()))
            {
                total_str = "??.?MB";
            }
            else
            {
                total_str = to_human_readable_filesize(static_cast<double>(progress_bar.total()), 1);
            }
            r.total.set_value(fmt::format("{:>7}", total_str));

            auto speed = progress_bar.speed();
            r.speed.set_value(fmt::format(
                "@ {:>7}/s",
                speed ? to_human_readable_filesize(static_cast<double>(speed), 1) : "??.?MB"
            ));

            r.separator.set_value("/");
        };
    }
}
