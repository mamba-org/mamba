#include <csignal>

#include "output.hpp"
#include "context.hpp"
#include "thirdparty/termcolor.hpp"

namespace mamba
{
    Context::Context()
    {
        set_verbosity(0);
        on_ci = (std::getenv("CI") != nullptr);
        if (on_ci || !termcolor::_internal::is_atty(std::cout))
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
        MessageLogger::global_log_severity() = mamba::LogSeverity::ERROR;
        if (lvl == 1)
        {
            MessageLogger::global_log_severity() = mamba::LogSeverity::INFO;
        }
        else if (lvl > 1)
        {
            MessageLogger::global_log_severity() = mamba::LogSeverity::DEBUG;
        }
        this->verbosity = lvl;
    }
}
