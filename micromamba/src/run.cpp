#include <reproc++/run.hpp>
#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
set_run_command(CLI::App* subcom)
{
    init_prefix_options(subcom);

    static std::vector<std::string> command;

    subcom->add_option(
        "executable_call",
        command,
        "Executable name, with additional arguments to be passed to the executable on invocation");

    subcom->callback([]() {
        auto& config = Configuration::instance();
        config.load();

        auto [wrapped_command, script_file]
            = prepare_wrapped_call(Context::instance().target_prefix, command);

        LOG_DEBUG << "Running wrapped script " << join(" ", command);
        std::string out, err;
        auto [_, ec] = reproc::run(wrapped_command,
                                   reproc::options(),
                                   reproc::sink::string(out),
                                   reproc::sink::string(err));

        if (ec)
        {
            std::cerr << ec.message() << std::endl;
        }
    });
}
