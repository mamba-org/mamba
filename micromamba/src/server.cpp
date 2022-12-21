// Server side C/C++ program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 8080

#include "microserver.cpp"
#include <spdlog/sinks/stdout_color_sinks.h>

#include "mamba/core/context.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/api/channel_loader.hpp"
#include "umamba.hpp"
#include "mamba/core/transaction.hpp"
#include "common_options.hpp"
#include "version.hpp"
#include <CLI/CLI.hpp>

struct cache
{
    mamba::MPool pool;
    std::chrono::time_point<std::chrono::system_clock> last_update;
};

using namespace mamba;

MPool
load_pool(const std::vector<std::string>& channels, MultiPackageCache& package_caches)
{
    auto& ctx = Context::instance();
    ctx.channels = channels;
    mamba::MPool pool;
    auto exp_load = load_channels(pool, package_caches, false);
    if (!exp_load)
    {
        throw std::runtime_error(exp_load.error().what());
    }
    return pool;
}

void
handle_solve_request(const microserver::Request& req, microserver::Response& res)
{
    auto& ctx = Context::instance();
    static std::map<std::string, cache> cache_map;

    auto j = nlohmann::json::parse(req.body);
    std::vector<std::string> specs = j["specs"].get<std::vector<std::string>>();
    std::vector<std::string> channels = j["channels"].get<std::vector<std::string>>();
    std::vector<std::string> virtual_packages
        = j["virtual_packages"].get<std::vector<std::string>>();

    for (const auto& s : specs)
    {
        if (auto m = MatchSpec{ s }; !m.channel.empty())
        {
            channels.push_back(m.channel);
        }
    }

    std::string cache_key = mamba::join(", ", channels);
    MultiPackageCache package_caches(ctx.pkgs_dirs);

    if (cache_map.find(cache_key) == cache_map.end())
    {
        cache_map[cache_key]
            = cache{ load_pool(channels, package_caches), std::chrono::system_clock::now() };
    }
    else
    {
        cache& c = cache_map[cache_key];
        if (std::chrono::system_clock::now() - c.last_update > std::chrono::minutes(30))
        {
            cache_map[cache_key]
                = cache{ load_pool(channels, package_caches), std::chrono::system_clock::now() };
        }
    }
    cache cache_entry = cache_map[cache_key];

    TemporaryDirectory tmp_dir;
    auto exp_prefix_data = PrefixData::create(tmp_dir.path());
    // if (!exp_prefix_data)
    // {
    // 	throw std::runtime_error(exp_prefix_data.error().what());
    // }
    PrefixData& prefix_data = exp_prefix_data.value();
    prefix_data.add_packages(get_virtual_packages());

    MRepo::create(cache_entry.pool, prefix_data);

    MSolver solver(
        cache_entry.pool,
        { { SOLVER_FLAG_ALLOW_UNINSTALL, ctx.allow_uninstall },
          { SOLVER_FLAG_ALLOW_DOWNGRADE, ctx.allow_downgrade },
          { SOLVER_FLAG_STRICT_REPO_PRIORITY, ctx.channel_priority == ChannelPriority::kStrict } });

    solver.add_jobs(specs, SOLVER_INSTALL);

    auto solved = solver.try_solve();

    if (!solved)
    {
        throw std::runtime_error("Could not solve");
    }

    MTransaction trans(solver, package_caches);
    auto to_install = std::get<1>(trans.to_conda());
    std::vector<nlohmann::json> packages;
    for (auto& p : to_install)
    {
        packages.push_back(nlohmann::json::parse(std::get<2>(p)));
    }
    nlohmann::json jout;
    jout["packages"] = packages;
    res.send(jout.dump());
}


int
run_server(int port)
{
    auto& config = mamba::Configuration::instance();
    config.load();
    auto server_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    server_sink->set_level(spdlog::level::debug);
    server_sink->set_pattern("%^[%H:%M:%S]%$ %v");

    spdlog::logger logger("server", { server_sink });

    microserver::Server xserver(logger);
    xserver.get("/hello",
                [](const microserver::Request& req, microserver::Response& res)
                { res.send("Hello World!"); });
    xserver.get("/",
                [](const microserver::Request& req, microserver::Response& res)
                {
                    res.type = "text/plain";
                    std::stringstream ss;
                    ss << banner << "\n\n"
                       << "Version " << UMAMBA_VERSION_STRING << "\n";
                    res.send(ss.str());
                });
    xserver.post("/solve", handle_solve_request);

    Console::stream() << "Starting server on port http://localhost:" << port << std::endl;

    xserver.start(port);
    return 0;
}

void
set_server_command(CLI::App* subcom)
{
    init_general_options(subcom);

    static int port = 1234;
    subcom->add_option("--port,-p", port, "The port to use for the server");

    subcom->callback([&]() { return run_server(port); });
}
