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
        ProgressProxy(ProgressBar* ptr,
                      std::size_t idx);
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

    private:

        ProgressBar* p_bar;
        std::size_t m_idx;
    };

    class ProgressBarManager
    {
    public:

        virtual ~ProgressBarManager() = default;
        
        // TODO: entity semantic

        virtual ProgressProxy add_progress_bar(const std::string& name) = 0;
        virtual void print_progress(std::size_t idx) = 0;
        virtual void deactivate_progress_bar(std::size_t idx, const std::string_view& msg = "") = 0;

        virtual void print(const std::string_view& str, bool skip_progress_bars) = 0;
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

        ProgressProxy add_progress_bar(const std::string& name) override;
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
    };


    class ProgressBar
    {
    public:

        virtual ~ProgressBar() = default;

        virtual void print() = 0;
        virtual void set_full() = 0;
        virtual void set_progress(size_t current, size_t total) = 0;

        // TODO: entity semantics
        void set_start();
        void set_postfix(const std::string& postfix_text);
        // TODO: remove this?
        //void mark_as_completed();
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

        DefaultProgressBar(const std::string& prefix);
        virtual ~DefaultProgressBar() = default;

        void print() override;
        void set_full() override;
        void set_progress(size_t current, size_t total) override;

    private:

        size_t m_progress;
    };

    class HiddenProgressBar : public ProgressBar
    {
    public:

        HiddenProgressBar(const std::string& prefix);
        virtual ~HiddenProgressBar() = default;

        void print() override;
        void set_full() override;
        void set_progress(size_t current, size_t total) override;
    };


}

#endif

