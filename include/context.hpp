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

    int verbosity = 4;

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
    }
};