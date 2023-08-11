// Server side C/C++ program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 8080

#include <optional>
#include <unordered_map>

#include <CLI/CLI.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/util/string.hpp"

#include "common_options.hpp"
#include "microserver.cpp"
#include "umamba.hpp"
#include "version.hpp"


using namespace mamba;

MPool
load_pool(
    const std::vector<std::string>& channels,
    MultiPackageCache& package_caches,
    mamba::ChannelContext& channel_context
)
{
    auto& ctx = channel_context.context();
    ctx.channels = channels;
    mamba::MPool pool{ channel_context };
    auto exp_load = load_channels(pool, package_caches, false);
    if (!exp_load)
    {
        throw std::runtime_error(exp_load.error().what());
    }
    return pool;
}

void
handle_solve_request(
    const microserver::Request& req,
    microserver::Response& res,
    mamba::ChannelContext& channel_context
)
{
    auto& ctx = channel_context.context();

    struct cache
    {
        std::optional<mamba::MPool> pool;
        std::chrono::time_point<std::chrono::system_clock> last_update;
    };

    static std::unordered_map<std::string, cache> cache_map;

    auto j = nlohmann::json::parse(req.body);
    std::vector<std::string> specs = j["specs"].get<std::vector<std::string>>();
    std::vector<std::string> channels = j["channels"].get<std::vector<std::string>>();
    std::vector<std::string> virtual_packages = j["virtual_packages"].get<std::vector<std::string>>();
    std::string platform = j["platform"];

    ctx.platform = platform;

    for (const auto& s : specs)
    {
        if (auto m = MatchSpec{ s, channel_context }; !m.channel.empty())
        {
            channels.push_back(m.channel);
        }
    }

    std::string cache_key = mamba::util::join(", ", channels) + fmt::format(", {}", platform);
    MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

    if (cache_map.find(cache_key) == cache_map.end())
    {
        cache_map.insert_or_assign(
            cache_key,
            cache{ load_pool(channels, package_caches, channel_context),
                   std::chrono::system_clock::now() }
        );
    }
    else
    {
        cache& c = cache_map[cache_key];
        if (std::chrono::system_clock::now() - c.last_update > std::chrono::minutes(30))
        {
            cache_map.insert_or_assign(
                cache_key,
                cache{ load_pool(channels, package_caches, channel_context),
                       std::chrono::system_clock::now() }
            );
        }
    }
    auto entry_it = cache_map.find(cache_key);
    if (entry_it == cache_map.end())
    {
        throw std::runtime_error("invalid cache state");
    }
    cache cache_entry = entry_it->second;

    TemporaryDirectory tmp_dir;
    auto exp_prefix_data = PrefixData::create(tmp_dir.path(), channel_context);
    // if (!exp_prefix_data)
    // {
    // 	throw std::runtime_error(exp_prefix_data.error().what());
    // }
    PrefixData& prefix_data = exp_prefix_data.value();
    std::vector<PackageInfo> vpacks;
    for (const auto& s : virtual_packages)
    {
        auto elements = util::split(s, "=");
        vpacks.push_back(detail::make_virtual_package(
            elements[0],
            ctx.platform,
            elements.size() >= 2 ? elements[1] : "",
            elements.size() >= 3 ? elements[2] : ""
        ));
    }
    prefix_data.add_packages(vpacks);

    auto installed_repo = MRepo(*cache_entry.pool, prefix_data);

    MSolver solver(
        *cache_entry.pool,
        { { SOLVER_FLAG_ALLOW_UNINSTALL, ctx.allow_uninstall },
          { SOLVER_FLAG_ALLOW_DOWNGRADE, ctx.allow_downgrade },
          { SOLVER_FLAG_STRICT_REPO_PRIORITY, ctx.channel_priority == ChannelPriority::kStrict } }
    );

    solver.add_jobs(specs, SOLVER_INSTALL);

    bool solved = solver.try_solve();

    if (!solved)
    {
        nlohmann::json jout;
        jout["error_msg"] = solver.problems_to_str();
        res.send(jout.dump());
    }
    else
    {
        MTransaction trans{ *cache_entry.pool, solver, package_caches };
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

    cache_entry.pool->remove_repo(installed_repo.id(), /* reuse_ids= */ true);
    pool_set_installed(*cache_entry.pool, nullptr);
}


int
run_server(int port, mamba::ChannelContext& channel_context, Configuration& config)
{
    config.load();
    std::signal(SIGPIPE, SIG_IGN);
    auto server_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    server_sink->set_level(spdlog::level::debug);
    server_sink->set_pattern("%^[%H:%M:%S]%$ %v");

    spdlog::logger logger("server", { server_sink });

    microserver::Server xserver(logger);
    xserver.get(
        "/hello",
        [](const microserver::Request&, microserver::Response& res) { res.send("Hello World!"); }
    );
    xserver.get(
        "/",
        [](const microserver::Request&, microserver::Response& res)
        {
            res.type = "text/plain";
            std::stringstream ss;
            ss << "Micromamba version " << UMAMBA_VERSION_STRING << "\n";
            res.send(ss.str());
        }
    );
    xserver.post(
        "/solve",
        [&](const microserver::Request& req, microserver::Response& res)
        { return handle_solve_request(req, res, channel_context); }
    );

    Console::stream() << "Starting server on port http://localhost:" << port << std::endl;

    xserver.start(port);
    return 0;
}

void
set_server_command(CLI::App* subcom, mamba::Configuration& config)
{
    init_general_options(subcom, config);

    static int port = 1234;
    subcom->add_option("--port,-p", port, "The port to use for the server");

    subcom->callback(
        [&config]
        {
            mamba::ChannelContext channel_context{ config.context() };
            return run_server(port, channel_context, config);
        }
    );
}
