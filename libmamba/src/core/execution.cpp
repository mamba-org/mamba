#include "mamba/core/execution.hpp"
#include "mamba/core/invoke.hpp"
#include "mamba/core/output.hpp"

namespace mamba
{
    // NOTE: see singleton.cpp for other functions and why they are located there instead of here

    void MainExecutor::invoke_close_handlers()
    {
        auto synched_handlers = close_handlers.synchronize();
        for (auto&& handler : *synched_handlers)
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
