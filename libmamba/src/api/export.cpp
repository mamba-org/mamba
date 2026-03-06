// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>
#include <string>

#include "mamba/api/configuration.hpp"
#include "mamba/api/env.hpp"
#include "mamba/api/environment_yaml.hpp"
#include "mamba/api/export.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/fs/filesystem.hpp"

namespace mamba
{
    void export_environment(Configuration& config)
    {
        auto& ctx = config.context();
        config.load();

        auto& file = config.at("file");
        auto& no_builds = config.at("no_builds").value<bool>();
        auto& ignore_channels = config.at("ignore_channels").value<bool>();

        if (ctx.prefix_params.target_prefix.empty())
        {
            throw std::runtime_error("No target prefix specified for export");
        }

        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        // Load prefix data
        auto maybe_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
        if (!maybe_prefix_data)
        {
            throw std::runtime_error(maybe_prefix_data.error().what());
        }
        PrefixData& prefix_data = maybe_prefix_data.value();

        // Get environment name
        std::string env_name = detail::get_env_name(ctx, ctx.prefix_params.target_prefix);

        // Convert prefix to yaml_file_contents
        auto yaml_contents = prefix_to_yaml_contents(
            prefix_data,
            ctx,
            env_name,
            { no_builds, ignore_channels, false }
        );

        // Determine output file path
        fs::u8path output_file;
        if (file.configured())
        {
            output_file = fs::u8path(file.value<std::string>());
        }
        else
        {
            output_file = fs::current_path() / "environment.yaml";
        }

        // Write to file
        yaml_contents_to_file(yaml_contents, output_file);

        if (ctx.output_params.json)
        {
            Console::instance().json_write({ { "success", true }, { "file", output_file.string() } });
        }
        else
        {
            Console::stream() << "Environment exported to: " << output_file.string() << "\n";
        }
    }
}
