#include <csignal>

// TODO: remove minilog
#include "minilog.hpp"
#include "context.hpp"

namespace mamba
{
    Context::Context()
    {
        set_verbosity(0);
        on_ci = (std::getenv("CI") != nullptr);
        if (on_ci)
        {
            no_progress_bars = true;
        }

        std::signal(SIGINT, [](int signum) {
            instance().sig_interrupt = true;
        });
    }

    Context& Context::instance()
    {
        static Context ctx;
        return ctx;
    }

    void Context::set_verbosity(int lvl)
    {
        if (lvl == 0)
        {
            minilog::global_log_severity = 3;
        }
        else if (lvl == 1)
        {
            minilog::global_log_severity = 1;
        }
        else 
        {
            minilog::global_log_severity = 0;
        }

        this->verbosity = lvl;
    }
}
