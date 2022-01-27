#include <reproc++/run.hpp>
#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
set_run_command(CLI::App* subcom)
{
    init_prefix_options(subcom);

    static bool live_stream = true;
    subcom->add_flag("--live-stream", live_stream, "Display output live");

    static std::vector<std::string> command;
    subcom->prefix_command();

    subcom->callback([subcom]() {
        auto& config = Configuration::instance();
        config.load();

        std::vector<std::string> command = subcom->remaining();

        auto [wrapped_command, script_file]
            = prepare_wrapped_call(Context::instance().target_prefix, command);

        LOG_DEBUG << "Running wrapped script " << join(" ", command);

        std::string out, err;
        if (!live_stream)
        {
            auto [_, ec] = reproc::run(wrapped_command,
                                       reproc::options(),
                                       reproc::sink::string(out),
                                       reproc::sink::string(err));
            if (ec)
            {
                std::cerr << ec.message() << std::endl;
            }
        }
        else
        {
            auto [_, ec] = reproc::run(wrapped_command, reproc::options());

            if (ec)
            {
                std::cerr << ec.message() << std::endl;
            }
        }
    });
}
