// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PROGRESS_BAR_HPP
#define MAMBA_CORE_PROGRESS_BAR_HPP

#include <string_view>

namespace mamba
{
    class ProgressBar;

    /*******************************
     * Public API of progress bars *
     *******************************/

    class ProgressProxy
    {
    public:
        ProgressProxy() = default;
        ProgressProxy(ProgressBar* ptr, std::size_t idx);
        ~ProgressProxy() = default;

        ProgressProxy(const ProgressProxy&) = default;
        ProgressProxy& operator=(const ProgressProxy&) = default;
        ProgressProxy(ProgressProxy&&) = default;
        ProgressProxy& operator=(ProgressProxy&&) = default;

        void set_full();
        void set_progress(size_t current, size_t total);
        void elapsed_time_to_stream(std::stringstream& s);
        void set_postfix(const std::string& s);
        void mark_as_completed(const std::string_view& final_message = "");
        void mark_as_extracted();

    private:
        ProgressBar* p_bar;
        std::size_t m_idx;
    };

    class ProgressBarManager
    {
    public:
        virtual ~ProgressBarManager() = default;

        ProgressBarManager(const ProgressBarManager&) = delete;
        ProgressBarManager& operator=(const ProgressBarManager&) = delete;
        ProgressBarManager(ProgressBarManager&&) = delete;
        ProgressBarManager& operator=(ProgressBarManager&&) = delete;

        virtual ProgressProxy add_progress_bar(const std::string& name, size_t expected_total = 0)
            = 0;
        virtual void print_progress(std::size_t idx) = 0;
        virtual void deactivate_progress_bar(std::size_t idx, const std::string_view& msg = "") = 0;

        virtual void print(const std::string_view& str, bool skip_progress_bars) = 0;

    protected:
        ProgressBarManager() = default;
    };

    enum ProgressBarMode
    {
        multi,
        aggregated
    };

    std::unique_ptr<ProgressBarManager> make_progress_bar_manager(ProgressBarMode);

    /******************************************
     * Internal use of progress bars,         *
     * should not be used directly by clients *
     ******************************************/

    class MultiBarManager : public ProgressBarManager
    {
    public:
        MultiBarManager();
        virtual ~MultiBarManager() = default;

        ProgressProxy add_progress_bar(const std::string& name, size_t expected_total) override;
        void print_progress(std::size_t idx) override;
        void deactivate_progress_bar(std::size_t idx, const std::string_view& msg = "") override;

        void print(const std::string_view& str, bool skip_progress_bars) override;

    private:
        void print_progress();

        using progress_bar_ptr = std::unique_ptr<ProgressBar>;
        std::vector<progress_bar_ptr> m_progress_bars;
        std::vector<ProgressBar*> m_active_progress_bars;
        bool m_progress_started;
    };

    class AggregatedBarManager : public ProgressBarManager
    {
    public:
        AggregatedBarManager();
        virtual ~AggregatedBarManager() = default;

        ProgressProxy add_progress_bar(const std::string& name, size_t expected_total) override;
        void print_progress(std::size_t idx) override;
        void deactivate_progress_bar(std::size_t idx, const std::string_view& msg = "") override;

        void print(const std::string_view& str, bool skip_progress_bars) override;

        void update_download_bar(std::size_t current_diff);
        void update_extract_bar();

    private:
        void print_progress();
        bool is_complete() const;

        std::chrono::time_point<std::chrono::high_resolution_clock> m_start_time;

        using progress_bar_ptr = std::unique_ptr<ProgressBar>;
        std::vector<progress_bar_ptr> m_progress_bars;
        progress_bar_ptr p_download_bar;
        progress_bar_ptr p_extract_bar;
        size_t m_completed;
        size_t m_extracted;
        std::mutex m_main_mutex;
        size_t m_current;
        size_t m_total;
        bool m_progress_started;
    };

    class ProgressBar
    {
    public:
        virtual ~ProgressBar() = default;

        ProgressBar(const ProgressBar&) = delete;
        ProgressBar& operator=(const ProgressBar&) = delete;
        ProgressBar(ProgressBar&&) = delete;
        ProgressBar& operator=(ProgressBar&&) = delete;
        virtual void print() = 0;
        virtual void set_full() = 0;
        virtual void set_progress(size_t current, size_t total) = 0;
        virtual void set_extracted() = 0;

        void set_start();
        void set_postfix(const std::string& postfix_text);
        void elapsed_time_to_stream(std::stringstream& s);
        const std::string& prefix() const;

    protected:
        ProgressBar(const std::string& prefix);

        std::chrono::nanoseconds m_elapsed_ns;
        std::chrono::time_point<std::chrono::high_resolution_clock> m_start_time;

        std::string m_prefix;
        std::string m_postfix;
        bool m_start_time_saved;
        bool m_activate_bob;
    };

    class DefaultProgressBar : public ProgressBar
    {
    public:
        DefaultProgressBar(const std::string& prefix, int width_cap = 20);
        virtual ~DefaultProgressBar() = default;

        void print() override;
        void set_full() override;
        void set_progress(size_t current, size_t total) override;
        void set_extracted() override;

    private:
        size_t m_progress;
        int m_width_cap;
    };

    class HiddenProgressBar : public ProgressBar
    {
    public:
        HiddenProgressBar(const std::string& prefix,
                          AggregatedBarManager* manager,
                          size_t expected_total);
        virtual ~HiddenProgressBar() = default;

        void print() override;
        void set_full() override;
        void set_progress(size_t current, size_t total) override;
        void set_extracted() override;

    private:
        AggregatedBarManager* p_manager;
        std::size_t m_current;
        std::size_t m_total;
    };
}

#endif
