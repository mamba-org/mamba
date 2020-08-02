#include "thread_utils.hpp"

#ifndef _WIN32
#include <signal.h>
#endif

#include <iostream>

namespace mamba
{

    /***********************
     * thread interruption *
     ***********************/

    namespace
    {
        std::atomic<bool> sig_interrupted;
    }

    void set_default_signal_handler()
    {
        std::signal(SIGINT, [](int signum)
        {
            set_sig_interrupted();
        });
    }

    bool is_sig_interrupted() noexcept
    {
        return sig_interrupted.load();
    }

    void set_sig_interrupted() noexcept
    {
        sig_interrupted.store(true);
    }

    void interruption_point()
    {
        if (is_sig_interrupted())
        {
            throw thread_interrupted();
        }
    }

    /*******************************
     * thread count implementation *
     *******************************/

    namespace
    {
        int thread_count = 0;
        std::mutex clean_mutex;
        std::condition_variable clean_var;

        std::atomic<bool> is_clean = false;
        std::mutex main_mutex;
        std::condition_variable main_var;
    }

    void increase_thread_count()
    {
        std::unique_lock<std::mutex> lk(clean_mutex);
        ++thread_count;
    }

    void decrease_thread_count()
    {
        std::unique_lock<std::mutex> lk(clean_mutex);
        --thread_count;
        std::notify_all_at_thread_exit(clean_var, std::move(lk));
    }

    int get_thread_count()
    {
        return thread_count;
    }

    void wait_before_cleaning()
    {
        std::unique_lock<std::mutex> lk(clean_mutex);
        clean_var.wait(lk, []() { return thread_count == 0; });
    }

    void notify_cleanup()
    {
        is_clean.store(true);
        main_var.notify_one();
    }

    void wait_for_cleanup()
    {
        std::unique_lock<std::mutex> lk(main_mutex);
        main_var.wait(lk, []() { return is_clean.load(); });
    }

    namespace
    {
        std::thread::native_handle_type cleanup_id;
    }

    void register_cleaning_thread_id(std::thread::native_handle_type id)
    {
        cleanup_id = id;
    }

    std::thread::native_handle_type get_cleaning_thread_id()
    {
        return cleanup_id;
    }

    /*************************
     * thread implementation *
     *************************/

    bool thread::joinable() const noexcept
    {
        return m_thread.joinable();
    }

    std::thread::id thread::get_id() const noexcept
    {
        return m_thread.get_id();
    }

    void thread::join()
    {
        m_thread.join();
    }

    void thread::detach()
    {
        m_thread.detach();
    }

    /**********************
     * interruption_guard *
     **********************/

#ifdef _WIN32

    std::function<void ()> interruption_guard::m_cleanup_function;

    interruption_guard::~interruption_guard()
    {
        set_default_signal_handler();
    }

#else

    namespace
    {
        sigset_t sigset;
    }

    interruption_guard::~interruption_guard()
    {
        if (is_sig_interrupted())
        {
            wait_for_cleanup();
        }
        m_interrupt.store(false);
        pthread_sigmask(SIG_UNBLOCK, &sigset, nullptr);
        set_default_signal_handler();
    }

    void interruption_guard::block_signals() const
    {
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGINT);
        pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
    }

    void interruption_guard::wait_for_signal() const
    {
        int signum = 0;
        sigwait(&sigset, &signum);
    }

#endif

}
