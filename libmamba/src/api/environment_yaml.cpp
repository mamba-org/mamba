// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <fstream>
#include <ios>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include "mamba/api/env.hpp"
#include "mamba/api/environment_yaml.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/util.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        // Helper function: Read environment variables from prefix state file
        // Converts UPPERCASE keys (from state file) to lowercase (for YAML)
        std::map<std::string, std::string> read_env_vars_from_prefix(const fs::u8path& prefix)
        {
            std::map<std::string, std::string> env_vars;
            const fs::u8path state_file_path = prefix / "conda-meta" / "state";

            if (fs::exists(state_file_path))
            {
                auto fin = open_ifstream(state_file_path);
                try
                {
                    nlohmann::json j;
                    fin >> j;
                    if (j.contains("env_vars") && j["env_vars"].is_object())
                    {
                        for (auto it = j["env_vars"].begin(); it != j["env_vars"].end(); ++it)
                        {
                            // Convert UPPERCASE keys from state file to lowercase for YAML
                            std::string key = it.key();
                            std::string lower_key = util::to_lower(key);
                            env_vars[lower_key] = it.value().get<std::string>();
                        }
                    }
                }
                catch (nlohmann::json::exception&)
                {
                    // If parsing fails, return empty map
                    LOG_DEBUG << "Could not read env_vars from state file: "
                              << state_file_path.string();
                }
            }

            return env_vars;
        }

        // Helper function: Extract channel name from channel string (URL or name)
        std::string
        extract_channel_name(ChannelContext& channel_context, const std::string& channel_str)
        {
            if (channel_str.empty())
            {
                return {};
            }

            // If it's already a simple channel name (no URL), return as-is
            if (channel_str.find("://") == std::string::npos)
            {
                return channel_str;
            }

            // Try to resolve the channel URL to get the channel name
            try
            {
                const auto& channels = channel_context.make_channel(channel_str);
                if (!channels.empty())
                {
                    return channels.front().id();
                }
            }
            catch (...)
            {
                // If resolution fails, try to extract channel name from URL
                // e.g., "https://conda.anaconda.org/conda-forge" -> "conda-forge"
                if (channel_str.find("conda.anaconda.org/") != std::string::npos)
                {
                    auto parts = util::rsplit(channel_str, '/');
                    if (parts.size() >= 2)
                    {
                        return parts[parts.size() - 1];
                    }
                }
            }

            return channel_str;
        }

        // Helper function: Extract channels from packages
        std::vector<std::string> extract_channels_from_packages(
            const PrefixData& prefix_data,
            ChannelContext& channel_context,
            bool ignore_channels = false
        )
        {
            if (ignore_channels)
            {
                return {};
            }

            std::set<std::string> channel_set;
            std::vector<std::string> channels;

            const auto records = prefix_data.sorted_records();
            for (const auto& pkg : records)
            {
                if (!pkg.channel.empty())
                {
                    std::string channel_name = extract_channel_name(channel_context, pkg.channel);
                    // Only add if not already seen (maintain order of first appearance)
                    if (!channel_set.contains(channel_name))
                    {
                        channel_set.insert(channel_name);
                        channels.push_back(channel_name);
                    }
                }
            }

            return channels;
        }

        // Helper function: Convert PackageInfo to MatchSpec string
        std::string package_to_spec_string(
            const specs::PackageInfo& pkg,
            ChannelContext& channel_context,
            bool no_builds = false,
            bool ignore_channels = false,
            bool include_md5 = false
        )
        {
            std::string spec;

            // Add channel prefix if not ignoring channels
            if (!ignore_channels && !pkg.channel.empty())
            {
                std::string channel_name = extract_channel_name(channel_context, pkg.channel);
                spec = channel_name + "::";
            }

            // Add package name
            spec += pkg.name;

            // Add version if available
            if (!pkg.version.empty())
            {
                spec += "=" + pkg.version;
            }

            // Add build string if not excluding builds
            if (!no_builds && !pkg.build_string.empty())
            {
                spec += "=" + pkg.build_string;
            }

            // Add md5 if requested (e.g. for env export --md5)
            if (include_md5 && !pkg.md5.empty())
            {
                spec += "[md5=" + pkg.md5 + "]";
            }

            return spec;
        }

        // Helper function: Download file from URL if needed
        std::unique_ptr<TemporaryFile>
        downloaded_file_from_url(const Context& ctx, const std::string& url_str)
        {
            if (url_str.find("://") != std::string::npos)
            {
                LOG_INFO << "Downloading file from " << url_str;
                auto url_parts = util::rsplit(url_str, '/');
                std::string filename = (url_parts.size() == 1) ? "" : url_parts.back();
                auto tmp_file = std::make_unique<TemporaryFile>("mambaf", util::concat("_", filename));
                download::Request request(
                    "Environment lock or yaml file",
                    download::MirrorName(""),
                    url_str,
                    tmp_file->path()
                );
                const download::Result res = download::download(
                    std::move(request),
                    ctx.mirrors,
                    ctx.remote_fetch_params,
                    ctx.authentication_info(),
                    ctx.download_options()
                );

                if (!res || res.value().transfer.http_status != 200)
                {
                    throw std::runtime_error(
                        fmt::format("Could not download environment lock or yaml file from {}", url_str)
                    );
                }

                return tmp_file;
            }
            return nullptr;
        }
    }  // namespace

    detail::yaml_file_contents prefix_to_yaml_contents(
        const PrefixData& prefix_data,
        const Context& ctx,
        const std::string& env_name,
        const PrefixToYamlOptions& options
    )
    {
        detail::yaml_file_contents result;

        // Create channel context to resolve channel names
        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        // Get environment name
        if (env_name.empty())
        {
            result.name = detail::get_env_name(ctx, prefix_data.path());
        }
        else
        {
            result.name = env_name;
        }

        // Set prefix
        result.prefix = prefix_data.path().string();

        // Extract channels from packages
        result.channels = extract_channels_from_packages(
            prefix_data,
            channel_context,
            options.ignore_channels
        );

        // Extract dependencies from installed packages
        const auto records = prefix_data.sorted_records();
        result.dependencies.reserve(records.size());
        for (const auto& pkg : records)
        {
            std::string spec = package_to_spec_string(
                pkg,
                channel_context,
                options.no_builds,
                options.ignore_channels,
                options.include_md5
            );
            result.dependencies.push_back(std::move(spec));
        }

        // Extract pip packages
        const auto& pip_records = prefix_data.pip_records();
        if (!pip_records.empty())
        {
            detail::other_pkg_mgr_spec pip_spec;
            pip_spec.pkg_mgr = "pip";
            pip_spec.cwd = prefix_data.path().string();
            pip_spec.deps.reserve(pip_records.size());

            for (const auto& [name, pkg] : pip_records)
            {
                std::string pip_dep = pkg.name;
                if (!pkg.version.empty())
                {
                    pip_dep += "==" + pkg.version;
                }
                pip_spec.deps.push_back(std::move(pip_dep));
            }

            result.others_pkg_mgrs_specs.push_back(std::move(pip_spec));

            // Add pip to dependencies if not already present
            if (std::find(result.dependencies.begin(), result.dependencies.end(), "pip")
                == result.dependencies.end())
            {
                result.dependencies.push_back("pip");
            }
        }

        // Read environment variables from state file
        result.variables = read_env_vars_from_prefix(prefix_data.path());

        return result;
    }

    void yaml_contents_to_stream(const detail::yaml_file_contents& contents, std::ostream& out)
    {
        YAML::Node root;

        // Add name if present
        if (!contents.name.empty())
        {
            root["name"] = contents.name;
        }

        // Add prefix if present
        if (!contents.prefix.empty())
        {
            root["prefix"] = contents.prefix;
        }

        // Always add channels (empty list when none) for consistent export structure
        root["channels"] = contents.channels;

        // Add dependencies
        YAML::Node deps_node;
        for (const auto& dep : contents.dependencies)
        {
            deps_node.push_back(dep);
        }

        // Add pip/uv dependencies as maps
        for (const auto& other_spec : contents.others_pkg_mgrs_specs)
        {
            if (other_spec.pkg_mgr == "pip" || other_spec.pkg_mgr == "uv")
            {
                YAML::Node pip_node;
                pip_node[other_spec.pkg_mgr] = other_spec.deps;
                deps_node.push_back(pip_node);
            }
        }

        // Always add dependencies (empty list when none) for consistent export structure
        root["dependencies"] = deps_node;

        // Add variables if present
        if (!contents.variables.empty())
        {
            root["variables"] = contents.variables;
        }

        // Write to stream
        YAML::Emitter emitter;
        emitter << root;
        out << emitter.c_str() << "\n";
    }

    void
    yaml_contents_to_file(const detail::yaml_file_contents& contents, const fs::u8path& yaml_file_path)
    {
        // Write to file using the stream function
        fs::create_directories(yaml_file_path.parent_path());
        std::ofstream out = open_ofstream(yaml_file_path);
        if (out.fail())
        {
            throw std::runtime_error("Couldn't open file for writing: " + yaml_file_path.string());
        }

        yaml_contents_to_stream(contents, out);
    }

    detail::yaml_file_contents file_to_yaml_contents(
        const Context& ctx,
        const std::string& yaml_file,
        const std::string& platform,
        bool use_uv
    )
    {
        // Download content of environment yaml file if URL
        auto tmp_yaml_file = downloaded_file_from_url(ctx, yaml_file);
        fs::u8path file;

        if (tmp_yaml_file)
        {
            file = tmp_yaml_file->path();
        }
        else
        {
            file = fs::weakly_canonical(util::expand_home(yaml_file));
            if (!fs::exists(file))
            {
                LOG_ERROR << "YAML spec file '" << file.string() << "' not found";
                throw std::runtime_error("File not found. Aborting.");
            }
        }

        detail::yaml_file_contents result;
        YAML::Node f;
        try
        {
            f = YAML::LoadFile(file.string());
        }
        catch (YAML::Exception& e)
        {
            LOG_ERROR << "YAML error in spec file '" << file.string() << "'";
            throw e;
        }

        YAML::Node deps;
        if (f["dependencies"] && f["dependencies"].IsSequence() && f["dependencies"].size() > 0)
        {
            deps = f["dependencies"];
        }
        else
        {
            // Empty or absent `dependencies` key
            deps = YAML::Node(YAML::NodeType::Null);
        }
        YAML::Node final_deps;

        bool has_pip_deps = false;
        for (auto it = deps.begin(); it != deps.end(); ++it)
        {
            if (it->IsScalar())
            {
                final_deps.push_back(*it);
            }
            else if (it->IsMap())
            {
                // we merge a map to the upper level if the selector works
                for (const auto& map_el : *it)
                {
                    std::string key = map_el.first.as<std::string>();
                    if (util::starts_with(key, "sel("))
                    {
                        bool selected = detail::eval_selector(key, platform);
                        if (selected)
                        {
                            const YAML::Node& rest = map_el.second;
                            if (rest.IsScalar())
                            {
                                final_deps.push_back(rest);
                            }
                            else
                            {
                                throw std::runtime_error(
                                    "Complicated selection merge not implemented yet."
                                );
                            }
                        }
                    }
                    else if (key == "pip" || key == "uv")
                    {
                        std::string yaml_parent_path;
                        if (tmp_yaml_file)  // yaml file is fetched remotely
                        {
                            yaml_parent_path = yaml_file;
                        }
                        else
                        {
                            yaml_parent_path = fs::absolute(yaml_file).parent_path().string();
                        }
                        result.others_pkg_mgrs_specs.push_back(
                            {
                                use_uv && key == "pip" ? "uv" : key,
                                map_el.second.as<std::vector<std::string>>(),
                                yaml_parent_path,
                            }
                        );
                        has_pip_deps = true;
                    }
                }
            }
        }

        std::vector<std::string> dependencies;
        try
        {
            if (final_deps.IsNull())
            {
                dependencies = {};
            }
            else
            {
                dependencies = final_deps.as<std::vector<std::string>>();
            }
        }
        catch (const YAML::Exception& e)
        {
            LOG_ERROR << "Bad conversion of 'dependencies' to a vector of string: " << final_deps;
            throw e;
        }

        // Check if pip/uv was explicitly in the scalar dependencies from the file
        bool has_pip_in_file = std::count(dependencies.begin(), dependencies.end(), "pip") > 0;
        bool has_uv_in_file = std::count(dependencies.begin(), dependencies.end(), "uv") > 0;

        if (has_pip_deps && use_uv && !has_uv_in_file)
        {
            dependencies.push_back("uv");
        }
        else if (has_pip_deps && has_uv_in_file)
        {
            for (auto& spec : result.others_pkg_mgrs_specs)
            {
                if (spec.pkg_mgr == "pip")
                {
                    spec.pkg_mgr = "uv";
                }
            }
        }
        // Add "pip" to dependencies if pip dependencies exist but "pip" is not in dependencies.
        // Do not add "pip" when the file already has "uv" (we use uv for pip deps in that case).
        if (has_pip_deps && !has_pip_in_file && !use_uv && !has_uv_in_file)
        {
            dependencies.push_back("pip");
        }

        result.dependencies = dependencies;

        if (f["channels"])
        {
            try
            {
                result.channels = f["channels"].as<std::vector<std::string>>();
            }
            catch (YAML::Exception& e)
            {
                LOG_ERROR << "Could not read 'channels' as vector of strings from '"
                          << file.string() << "'";
                throw e;
            }
        }
        else
        {
            LOG_DEBUG << "No 'channels' specified in YAML spec file '" << file.string() << "'";
        }

        if (f["name"])
        {
            result.name = f["name"].as<std::string>();
        }
        else
        {
            LOG_DEBUG << "No env 'name' specified in YAML spec file '" << file.string() << "'";
        }

        if (f["variables"])
        {
            result.variables = f["variables"].as<std::map<std::string, std::string>>();
        }
        else
        {
            LOG_DEBUG << "No 'variables' specified in YAML spec file '" << file.string() << "'";
        }

        if (f["prefix"])
        {
            result.prefix = f["prefix"].as<std::string>();
        }
        else
        {
            LOG_DEBUG << "No 'prefix' specified in YAML spec file '" << file.string() << "'";
        }

        return result;
    }
}
