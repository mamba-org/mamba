#include "mamba/core/execution.hpp"

#include <atomic>
#include <mutex>
#include <cassert>

namespace mamba
{
    static std::atomic<MainExecutor*> main_executor{ nullptr };

    static std::mutex default_executor_mutex;  // TODO: replace by sychronized_value once available
    static std::unique_ptr<MainExecutor> default_executor;

    MainExecutor& MainExecutor::instance()
    {
        if (!main_executor)
        {
            // When no MainExecutor was created before we create a static one.
            std::scoped_lock lock{ default_executor_mutex };
            if (!main_executor)  // double check necessary to avoid data race
            {
                default_executor = std::make_unique<MainExecutor>();
                assert(main_executor == default_executor.get());
            }
        }

        return *main_executor;
    }

    void MainExecutor::stop_default()
    {
        std::scoped_lock lock{ default_executor_mutex };
        default_executor.reset();
    }

    MainExecutor::MainExecutor()
    {
        MainExecutor* expected = nullptr;
        if (!main_executor.compare_exchange_strong(expected, this))
            throw MainExecutorError("attempted to create multiple main executors");
    }

    MainExecutor::~MainExecutor()
    {
        close();
        main_executor = nullptr;
    }

}
