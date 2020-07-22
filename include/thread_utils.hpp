// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_THREAD_UTILS_HPP
#define MAMBA_THREAD_UTILS_HPP

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

namespace mamba
{
    /***********************
     * thread interruption *
     ***********************/

    bool is_interrupted() noexcept;
    void set_interrupted() noexcept;

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
    // Ensures the cleaning thread won't free ressources
    // that could be required by threads still active.
    void wait_before_cleaning();
 
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

    /***********
     * cleaner *
     ***********/

    /*class victor
    {
    public:

        victor();
        ~victor();

        victor(const victor&) = delete;
        victor& operator=(const victor&) = delete;

        victor(victor&&) = delete;
        victor& operator=(victor&&) = delete;
    };*/
}

#endif

