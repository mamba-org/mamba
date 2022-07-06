// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PROGRESS_BAR_IMPL_HPP
#define MAMBA_CORE_PROGRESS_BAR_IMPL_HPP

#include "spdlog/spdlog.h"
#ifdef SPDLOG_FMT_EXTERNAL
#include "fmt/color.h"
#else
#include "spdlog/fmt/bundled/color.h"
#endif

#include <iosfwd>
#include <mutex>
#include <atomic>
#include <set>
#include <string_view>
#include <vector>
#include <map>

#include "mamba/core/progress_bar.hpp"

namespace mamba
{
    void to_human_readable_filesize(std::ostream& o, double bytes, std::size_t precision = 0);
    std::string to_human_readable_filesize(double bytes, std::size_t precision);

    std::ostream& write_duration(std::ostream& os, std::chrono::nanoseconds ns);
    std::stringstream duration_stream(std::chrono::nanoseconds ns);
    std::string duration_str(std::chrono::nanoseconds ns);

    int get_console_width();
    int get_console_height();

    enum ChronoState
    {
        unset = 0,
        started = 1,
        paused = 2,
        stopped = 3,
        terminated = 4
    };

    class Chrono
    {
    public:
        using duration_t = std::chrono::milliseconds;
        using time_point_t
            = std::chrono::time_point<std::chrono::high_resolution_clock, duration_t>;

        Chrono() = default;

        ChronoState status() const;
        bool started() const;
        bool paused() const;
        bool stopped() const;
        bool terminated() const;
        bool unset() const;

        void start();
        void start(const time_point_t& time_point);
        void pause();
        void resume();
        void stop();
        void terminate();

        void set_start_time(const time_point_t& time_point);
        time_point_t start_time() const;
        time_point_t last_active_time();

        void set_elapsed_time(const duration_t& time);
        std::string elapsed_time_to_str();
        duration_t elapsed();

        std::unique_lock<std::mutex> chrono_lock();

    private:
        time_point_t m_start;
        duration_t m_elapsed_ns = duration_t::zero();
        ChronoState m_state = ChronoState::unset;
        std::mutex m_mutex;

        void compute_elapsed();

    protected:
        static time_point_t now();
    };

    class FieldRepr
    {
    public:
        bool active() const;
        bool defined() const;
        operator bool() const;
        bool overflow() const;

        std::string formatted_value(bool allow_overflow = false) const;
        std::size_t width(bool allow_overflow = true) const;
        std::size_t stored_width() const;
        std::string value() const;

        FieldRepr& activate();
        FieldRepr& deactivate();
        FieldRepr& set_format(const std::string& str);
        FieldRepr& set_format(const std::string& str, std::size_t size);
        FieldRepr& set_value(const std::string& str);
        FieldRepr& set_width(std::size_t size);
        FieldRepr& reset_width();
        FieldRepr& resize(std::size_t size);

    private:
        std::string m_value = "";
        std::size_t m_width = 0;
        std::string m_format = "";
        bool m_active = true;

        static std::string resize(const std::string& str, std::size_t size);
    };

    class ProgressBar;
    class ProgressBarManager;

    class ProgressBarRepr
    {
    public:
        ProgressBarRepr() = default;
        ProgressBarRepr(ProgressBar* pbar);

        FieldRepr prefix, progress, current, separator, total, speed, postfix, elapsed;
        fmt::text_style style;

        void print(std::ostream& ostream, std::size_t width = 0, bool with_endl = true);

        void compute_progress();
        void compute_progress_width();
        void compute_progress_value();

        ProgressBarRepr& set_width(std::size_t width);
        std::size_t width() const;

        ProgressBarRepr& reset_fields();

        ProgressBar& progress_bar();

    private:
        ProgressBar* p_progress_bar = nullptr;
        std::size_t m_width = 0;

        void set_same_widths(const ProgressBarRepr& r);
        void deactivate_empty_fields();
        std::vector<FieldRepr*> fields();

        friend class ProgressBar;
        friend class ProgressBarManager;
    };

    class ProgressBarManager;

    /*******************************
     * Public API of progress bars *
     *******************************/

    class ProgressBarManager : public Chrono
    {
    public:
        virtual ~ProgressBarManager();

        ProgressBarManager(const ProgressBarManager&) = delete;
        ProgressBarManager& operator=(const ProgressBarManager&) = delete;
        ProgressBarManager(ProgressBarManager&&) = delete;
        ProgressBarManager& operator=(ProgressBarManager&&) = delete;

        virtual ProgressProxy add_progress_bar(const std::string& name, size_t expected_total = 0)
            = 0;
        virtual void clear_progress_bars();
        virtual void add_label(const std::string& label, const ProgressProxy& progress_bar);

        void watch_print(const duration_t& period = std::chrono::milliseconds(100));
        virtual std::size_t print(std::ostream& os,
                                  std::size_t width = 0,
                                  std::size_t max_lines = std::numeric_limits<std::size_t>::max(),
                                  bool with_endl = true)
            = 0;
        void start();
        void terminate();

        void register_print_hook(std::function<void(std::ostream&)> f);
        void register_pre_start_hook(std::function<void()> f);
        void register_post_stop_hook(std::function<void()> f);

        void compute_bars_progress(std::vector<ProgressBar*>& bars);

        void activate_sorting();
        void deactivate_sorting();

    protected:
        using progress_bar_ptr = std::unique_ptr<ProgressBar>;

        ProgressBarManager() = default;
        ProgressBarManager(std::size_t width);

        duration_t m_period = std::chrono::milliseconds(100);
        std::vector<progress_bar_ptr> m_progress_bars = {};
        std::map<std::string, std::vector<ProgressBar*>> m_labels;
        std::atomic<bool> m_marked_to_terminate{ false };
        std::atomic<bool> m_watch_print_started{ false };
        bool m_sort_bars = false;
        std::size_t m_width = 0;

        std::mutex m_mutex;

        ProgressBar* raw_bar(const ProgressProxy& progress_bar);

        void run();

        std::vector<std::function<void(std::ostream&)>> m_print_hooks;
        std::vector<std::function<void()>> m_pre_start_hooks;
        std::vector<std::function<void()>> m_post_stop_hooks;

        void erase_lines(std::ostream& ostream, std::size_t count);
        void call_print_hooks(std::ostream& ostream);
        void sort_bars(bool max_height_exceeded);
    };


    std::unique_ptr<ProgressBarManager> make_progress_bar_manager(ProgressBarMode);


    /*********************************
     * Internal use of progress bars *
     *********************************/

    class MultiBarManager : public ProgressBarManager
    {
    public:
        MultiBarManager();
        MultiBarManager(std::size_t width);
        virtual ~MultiBarManager() = default;

        ProgressProxy add_progress_bar(const std::string& name, size_t expected_total) override;

        std::size_t print(std::ostream& os,
                          std::size_t width = 0,
                          std::size_t max_lines = std::numeric_limits<std::size_t>::max(),
                          bool with_endl = true) override;
    };

    class AggregatedBarManager : public ProgressBarManager
    {
    public:
        AggregatedBarManager();
        AggregatedBarManager(std::size_t width);
        virtual ~AggregatedBarManager() = default;

        ProgressProxy add_progress_bar(const std::string& name,
                                       std::size_t expected_total) override;

        void update_download_bar(std::size_t current_diff);
        void update_extract_bar();

        void add_label(const std::string& label, const ProgressProxy& progress_bar) override;
        void clear_progress_bars() override;

        void activate_sub_bars();
        void deactivate_sub_bars();

        ProgressBar* aggregated_bar(const std::string& label);

        std::size_t print(std::ostream& os,
                          std::size_t width = 0,
                          std::size_t max_lines = std::numeric_limits<std::size_t>::max(),
                          bool with_endl = true) override;

    private:
        std::map<std::string, progress_bar_ptr> m_aggregated_bars;
        bool m_print_sub_bars = false;

        bool is_complete() const;

        void update_aggregates_progress();
    };

    class ProgressBar : public Chrono
    {
    public:
        virtual ~ProgressBar();

        ProgressBar(const ProgressBar&) = delete;
        ProgressBar& operator=(const ProgressBar&) = delete;
        ProgressBar(ProgressBar&&) = delete;
        ProgressBar& operator=(ProgressBar&&) = delete;

        virtual void print(std::ostream& stream, std::size_t width = 0, bool with_endl = true) = 0;

        ProgressBarRepr& update_repr(bool compute_bar = true);
        const ProgressBarRepr& repr() const;
        ProgressBarRepr& repr();

        ProgressBar& set_progress(std::size_t current, std::size_t total);
        ProgressBar& update_progress(std::size_t current, std::size_t total);
        ProgressBar& set_progress(double progress);
        ProgressBar& set_current(std::size_t current);
        ProgressBar& set_in_progress(std::size_t in_progress);
        ProgressBar& update_current(std::size_t current);
        ProgressBar& set_total(std::size_t total);
        ProgressBar& set_speed(std::size_t speed);
        ProgressBar& set_full();
        ProgressBar& activate_spinner();
        ProgressBar& deactivate_spinner();

        ProgressBar& mark_as_completed(const std::chrono::milliseconds& delay
                                       = std::chrono::milliseconds::zero());

        std::size_t current() const;
        std::size_t in_progress() const;
        std::size_t total() const;
        std::size_t speed() const;
        std::size_t avg_speed(const std::chrono::milliseconds& ref_duration
                              = std::chrono::milliseconds::max());
        double progress() const;
        bool completed() const;
        bool is_spinner() const;

        const std::set<std::string>& active_tasks() const;
        std::set<std::string>& active_tasks();
        const std::set<std::string>& all_tasks() const;
        std::set<std::string>& all_tasks();
        ProgressBar& add_active_task(const std::string& name);
        ProgressBar& add_task(const std::string& name);
        std::string last_active_task();

        ProgressBar& set_repr_hook(std::function<void(ProgressBarRepr&)> f);
        ProgressBar& set_progress_hook(std::function<void(ProgressProxy&)> f);

        ProgressBar& set_prefix(const std::string& str);
        ProgressBar& set_postfix(const std::string& str);

        std::string prefix() const;

        int width() const;

    protected:
        ProgressBar(const std::string& prefix, std::size_t total, int width = 0);

        double m_progress = 0.;
        std::size_t m_current = 0;
        std::size_t m_in_progress = 0;
        std::size_t m_total = 0;
        std::size_t m_speed = 0, m_avg_speed = 0, m_current_avg = 0;
        int m_width = 0;

        std::set<std::string> m_active_tasks = {};
        std::set<std::string> m_all_tasks = {};
        std::string m_last_active_task = "";
        time_point_t m_task_time, m_avg_speed_time;

        ProgressBarRepr m_repr;

        bool m_is_spinner;
        bool m_completed = false;

        std::mutex m_mutex;

        void run();

        std::function<void(ProgressBarRepr&)> p_repr_hook;
        std::function<void(ProgressProxy&)> p_progress_hook;

        ProgressBar& call_progress_hook();
        ProgressBar& call_repr_hook();
    };

    class DefaultProgressBar : public ProgressBar
    {
    public:
        DefaultProgressBar(const std::string& prefix, std::size_t total, int width = 0);
        virtual ~DefaultProgressBar() = default;

        void print(std::ostream& stream, std::size_t width = 0, bool with_endl = true) override;
    };

    class HiddenProgressBar : public ProgressBar
    {
    public:
        HiddenProgressBar(const std::string& prefix,
                          AggregatedBarManager* manager,
                          std::size_t total,
                          int width = 0);
        virtual ~HiddenProgressBar() = default;

        void print(std::ostream& stream, std::size_t width = 0, bool with_endl = true) override;
    };
}  // namespace mamba

#endif
