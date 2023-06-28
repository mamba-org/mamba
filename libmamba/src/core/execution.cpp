#include "mamba/core/execution.hpp"
#include "mamba/core/invoke.hpp"
#include "mamba/core/output.hpp"

namespace mamba
{
    // NOTE: see singleton.cpp for other functions and why they are located there instead of here

    void MainExecutor::invoke_close_handlers()
    {
        std::scoped_lock lock{ handlers_mutex };
        for (auto&& handler : close_handlers)
        {
            const auto result = safe_invoke(handler);
            if (!result)
            {
                LOG_ERROR << "main executor close handler failed (ignored): "
                          << result.error().what();
            }
        }
    }
}
