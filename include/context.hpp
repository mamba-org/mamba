#ifndef MAMBA_CONTEXT_HPP
#define MAMBA_CONTEXT_HPP

#include <vector>
#include <string>

namespace mamba
{
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
        int verbosity = 0;

        bool on_ci = false;
        bool no_progress_bars = false;
        bool dry_run = false;
        std::string target_prefix;
        bool always_yes = false;

        bool sig_interrupt = false;

        int retry_timeout = 2; // seconds
        int retry_backoff = 3; // retry_timeout * retry_backoff
        int max_retries = 3;  // max number of retries

        // ssl verify can be either an empty string (regular SSL verification),
        // the string "<false>" to indicate no SSL verification, or a path to 
        // a directory with cert files, or a cert file.
        std::string ssl_verify = "";

        void set_verbosity(int lvl);
    
        static Context& instance();

        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;

        Context(Context&&) = delete;
        Context& operator=(Context&&) = delete;

    private:

        Context();
        ~Context() = default;
    };
}

#endif // MAMBA_CONTEXT_HPP
