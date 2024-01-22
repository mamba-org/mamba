// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <solv/solver.h>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/update.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    void update(Configuration& config, bool update_all, bool prune_deps, bool remove_not_specified)
    {
        auto& ctx = config.context();

        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
        config.load();

        const auto& raw_update_specs = config.at("specs").value<std::vector<std::string>>();

        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        // add channels from specs
        for (const auto& s : raw_update_specs)
        {
            if (auto m = specs::MatchSpec::parse(s); m.channel().has_value())
            {
                ctx.channels.push_back(m.channel()->str());
            }
        }

        MPool pool{ ctx, channel_context };
        MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

        auto exp_loaded = load_channels(ctx, pool, package_caches);
        if (!exp_loaded)
        {
            throw std::runtime_error(exp_loaded.error().what());
        }

        auto exp_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
        if (!exp_prefix_data)
        {
            // TODO: propagate tl::expected mechanism
            throw std::runtime_error(exp_prefix_data.error().what());
        }
        PrefixData& prefix_data = exp_prefix_data.value();

        std::vector<std::string> prefix_pkgs;
        for (auto& it : prefix_data.records())
        {
            prefix_pkgs.push_back(it.first);
        }

        load_installed_packages_in_pool(ctx, pool, prefix_data);

        MSolver solver(
            pool,
            {
                { SOLVER_FLAG_ALLOW_DOWNGRADE, ctx.allow_downgrade },
                { SOLVER_FLAG_ALLOW_UNINSTALL, ctx.allow_uninstall },
                { SOLVER_FLAG_STRICT_REPO_PRIORITY, ctx.channel_priority == ChannelPriority::Strict },
            }
        );

        auto& no_pin = config.at("no_pin").value<bool>();
        auto& no_py_pin = config.at("no_py_pin").value<bool>();

        if (!no_pin)
        {
            for (const auto& pin : file_pins(prefix_data.path() / "conda-meta" / "pinned"))
            {
                solver.add_pin(specs::MatchSpec::parse(pin));
            }
            for (const auto& pin : ctx.pinned_packages)
            {
                solver.add_pin(specs::MatchSpec::parse(pin));
            }
        }

        if (!no_py_pin)
        {
            auto py_pin = python_pin(prefix_data, raw_update_specs);
            if (!py_pin.empty())
            {
                solver.add_pin(specs::MatchSpec::parse(py_pin));
            }
        }
        if (!solver.pinned_specs().empty())
        {
            std::vector<std::string> pinned_str;
            for (auto& ms : solver.pinned_specs())
            {
                pinned_str.push_back("  - " + ms.conda_build_form() + "\n");
            }
            Console::instance().print("\nPinned packages:\n" + util::join("", pinned_str));
        }

        // FRAGILE this must be called after pins be before jobs in current ``MPool``
        pool.create_whatprovides();

        auto request = Request();

        if (update_all)
        {
            if (prune_deps)
            {
                auto hist_map = prefix_data.history().get_requested_specs_map();
                request.items.reserve(hist_map.size() + 1);

                for (auto& [name, spec] : hist_map)
                {
                    request.items.emplace_back(Request::Keep{ std::move(spec) });
                }
                request.items.emplace_back(Request::UpdateAll{ /* .clean_dependencies= */ true });
            }
            else
            {
                request.items.emplace_back(Request::UpdateAll{ /* .clean_dependencies= */ false });
            }
        }
        else
        {
            request.items.reserve(raw_update_specs.size());
            if (remove_not_specified)
            {
                auto hist_map = prefix_data.history().get_requested_specs_map();
                for (auto& it : hist_map)
                {
                    if (std::find(raw_update_specs.begin(), raw_update_specs.end(), it.second.name().str())
                        == raw_update_specs.end())
                    {
                        request.items.emplace_back(Request::Remove{
                            specs::MatchSpec::parse(it.second.name().str()),
                            /* .clean_dependencies= */ true,
                        });
                    }
                }
            }

            for (const auto& raw_ms : raw_update_specs)
            {
                request.items.emplace_back(Request::Update{
                    specs::MatchSpec::parse(raw_ms),
                });
            }
        }

        solver.add_request(request);

        solver.must_solve();

        auto execute_transaction = [&](MTransaction& transaction)
        {
            if (ctx.output_params.json)
            {
                transaction.log_json();
            }

            bool yes = transaction.prompt();
            if (yes)
            {
                transaction.execute(prefix_data);
            }
        };

        MTransaction transaction(pool, solver, package_caches);
        execute_transaction(transaction);
    }
}
