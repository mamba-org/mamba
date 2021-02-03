// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#include "mamba/thread_utils.hpp"

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
        std::atomic<bool> sig_interrupted(false);
        std::mutex sig_interrupted_cv_mutex;
        std::condition_variable sig_interrupted_cv;
    }

#ifndef _WIN32
    namespace
    {
        std::thread::native_handle_type sig_recv_thread;
    }

    void reset_sig_interrupted()
    {
        sig_interrupted.store(false);
        set_default_signal_handler();
    }

    std::thread::native_handle_type get_signal_receiver_thread_id()
    {
        return sig_recv_thread;
    }

    int default_signal_handler(sigset_t sigset)
    {
        int signum = 0;
        // wait until a signal is delivered:
        sigwait(&sigset, &signum);
        sig_interrupted.store(true);
        // notify all waiting workers to check their predicate:
        sig_interrupted_cv.notify_all();
        return signum;
    }

    void set_default_signal_handler()
    {
        // block signals in this thread and subsequently
        // spawned threads
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGINT);
        // sigaddset(&sigset, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
        std::thread receiver(default_signal_handler, sigset);
        sig_recv_thread = receiver.native_handle();
        receiver.detach();
    }
#else
    void set_default_signal_handler()
    {
        std::signal(SIGINT, [](int signum) { set_sig_interrupted(); });
    }
#endif

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
    }  // namespace

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

    void wait_for_all_threads()
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

    std::function<void()> interruption_guard::m_cleanup_function;

    interruption_guard::~interruption_guard()
    {
        wait_for_all_threads();
        if (is_sig_interrupted())
        {
            m_cleanup_function();
        }
    }

}  // namespace mamba
