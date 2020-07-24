// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_THREAD_UTILS_HPP
#define MAMBA_THREAD_UTILS_HPP

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

namespace mamba
{

#ifdef MAMBA_TEST_SUITE
#endif

    /***********************
     * thread interruption *
     ***********************/

    void set_default_signal_handler();

    bool is_sig_interrupted() noexcept;
    void set_sig_interrupted() noexcept;

    void interruption_point();

    class thread_interrupted : public std::exception
    {
    public:

        thread_interrupted() = default;
    };

    /****************
     * thread count *
     ****************/

    void increase_thread_count();
    void decrease_thread_count();
    int get_thread_count();

    // Waits until all other threads have finished
    // Must be called by the cleaning thread to ensure
    // it won't free ressources that could be required
    // by threads still active.
    void wait_before_cleaning();
    void notify_cleanup();

    // Should be called by the main thread to ensure
    // it does not exit before the cleaning thread has
    // terminated.
    void wait_for_cleanup();

    void register_cleaning_thread_id(std::thread::native_handle_type);
    std::thread::native_handle_type get_cleaning_thread_id();

    /**********
     * thread *
     **********/

    // Thread that increases the threads count upon
    // creation and decreases it upon deletion. Use it
    // when you need to ensure all threads have exited
    class thread
    {
    public:

        thread() = default;
        ~thread() = default;

        thread(const thread&) = delete;
        thread& operator=(const thread&) = delete;

        thread(thread&&) noexcept = default;
        thread& operator=(thread&&) = default;

        template <class Function, class... Args>
        explicit thread(Function&& func, Args&&... args);

        bool joinable() const noexcept;
        std::thread::id get_id() const noexcept;

        void join();
        void detach();

    private:

        std::thread m_thread;
    };

    template <class Function, class... Args>
    inline thread::thread(Function&& func, Args&&... args)
    {
        auto f = std::bind(std::forward<Function>(func), std::forward<Args>(args)...);
        m_thread = std::thread([f]()
        {
            increase_thread_count();
            try
            {
                f();
            }
            catch(thread_interrupted&)
            {
            }
            decrease_thread_count();
        });
    }

    /**********************
     * interruption_guard *
     **********************/

    class interruption_guard
    {
    public:

        template <class Function, class... Args>
        interruption_guard(Function&& func, Args&&... args);
        ~interruption_guard();

        interruption_guard(const interruption_guard&) = delete;
        interruption_guard& operator=(const interruption_guard&) = delete;

        interruption_guard(interruption_guard&&) = delete;
        interruption_guard& operator=(interruption_guard&&) = delete;

    private:

#ifdef _WIN32
        static std::function<void ()> m_cleanup_function;
#else
        void block_signals() const;
        void wait_for_signal() const;
        std::atomic<bool> m_interrupt;
#endif
    };

#ifdef _WIN32

    template <class Function, class... Args>
    inline interruption_guard::interruption_guard(Function&& func, Args&&... args)
    {
        m_cleanup_function = std::bind(std::forward<Function>(func), std::forward<Args>(args)...);
        std::signal(SIGINT, [](int) {
            set_sig_interrupted();
            wait_before_cleaning();
            m_cleanup_function();
        });
    }

#else

    template <class Function, class... Args>
    inline interruption_guard::interruption_guard(Function&& func, Args&&... args)
        : m_interrupt(true)
    {
        block_signals();
        auto f = std::bind(std::forward<Function>(func), std::forward<Args>(args)...);
        std::thread victor_the_cleaner([f, this]()
        {
            wait_for_signal();
            if (m_interrupt.load())
            {
                set_sig_interrupted();
                wait_before_cleaning();
                f();
                notify_cleanup();
            }
        });
        register_cleaning_thread_id(victor_the_cleaner.native_handle());
        victor_the_cleaner.detach();
    };

#endif

}

#endif

