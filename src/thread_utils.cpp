#include "thread_utils.hpp"

#ifndef _WIN32
#include <signal.h>
#endif

namespace mamba
{
    /***********************
     * thread interruption *
     ***********************/

    namespace
    {
        std::atomic<bool> sig_interrupted;
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
#else 
    namespace
    {
        sigset_t sigset;
    }

    void interruption_guard::block_signals() const
    {
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGINT);
        pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
    }

    void interruption_guard::reset_signal_handler() const
    {
        pthread_sigmask(SIG_UNBLOCK, &sigset, nullptr);
        std::signal(SIGINT, [](int signum) {
            set_sig_interrupted();
        });
    }

    void interruption_guard::wait_for_signal() const
    {
        int signum = 0;
        sigwait(&sigset, &signum);
    }

#endif

}

