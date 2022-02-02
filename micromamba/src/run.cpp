#include <reproc++/run.hpp>
#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
set_run_command(CLI::App* subcom)
{
    init_prefix_options(subcom);

    static std::string streams;
    CLI::Option* stream_option = subcom->add_option("-a,--attach", streams, "Attach to stdin, stdout and/or stderr. -a \"\" for disabling stream redirection")->join(',');

    static std::vector<std::string> command;
    subcom->prefix_command();

    subcom->callback([subcom, stream_option]() {
        auto& config = Configuration::instance();
        config.load();

        std::vector<std::string> command = subcom->remaining();

        auto [wrapped_command, script_file]
            = prepare_wrapped_call(Context::instance().target_prefix, command);

        LOG_DEBUG << "Running wrapped script " << join(" ", command);

        std::string out, err;

        bool all_streams = stream_option->count() == 0u;
        bool sinkout = !all_streams && streams.find("stdout") == std::string::npos;
        bool sinkerr = !all_streams && streams.find("stderr") == std::string::npos;

        reproc::options opt;
        opt.redirect.out.type = sinkout ? reproc::redirect::discard : reproc::redirect::parent;
        opt.redirect.err.type = sinkerr ? reproc::redirect::discard : reproc::redirect::parent;
        auto [_, ec] = reproc::run(wrapped_command, opt);

        if (ec)
        {
            std::cerr << ec.message() << std::endl;
        }
    });
}
