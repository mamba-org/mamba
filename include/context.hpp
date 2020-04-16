#pragma once

#include <vector>
#include <string>
#include <memory>

// Context singleton class
class Context
{
public:

    // TODO $CONDA_PREFIX doesn't work.
    std::vector<std::string> pkgs_dirs = {"$CONDA_PREFIX/pkgs"};

    bool use_index_cache = false;
    std::size_t local_repodata_ttl = 1; // take from header
    bool offline = false;
    bool quiet = false;
    bool json = false;

    long max_parallel_downloads = 5;
    int verbosity = 4;

    bool on_ci = false;
    bool no_progress_bars = false;
    bool dry_run = false;
    bool always_yes = false;

    void set_verbosity(int lvl)
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

    Context(Context const&) = delete;
    Context& operator=(Context const&) = delete;

    Context& operator()()
    {
        return instance();
    }

    static Context& instance()
    {
        static Context ctx;
        return ctx;
    }

private:
    Context() {
        set_verbosity(0);
        on_ci = (std::getenv("CI") != nullptr);
        if (on_ci)
        {
            no_progress_bars = true;
        }
    }
};
