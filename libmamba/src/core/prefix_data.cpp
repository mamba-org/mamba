// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <reproc++/run.hpp>
#include <fmt/ranges.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/graph.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    auto PrefixData::create(const fs::u8path& prefix_path, ChannelContext& channel_context)
        -> expected_t<PrefixData>
    {
        try
        {
            return PrefixData(prefix_path, channel_context);
        }
        catch (std::exception& e)
        {
            return tl::make_unexpected(mamba_error(e.what(), mamba_error_code::prefix_data_not_loaded)
            );
        }
        catch (...)
        {
            return tl::make_unexpected(mamba_error(
                "Unknown error when trying to load prefix data " + prefix_path.string(),
                mamba_error_code::unknown
            ));
        }
    }

    PrefixData::PrefixData(const fs::u8path& prefix_path, ChannelContext& channel_context)
        : m_history(prefix_path, channel_context)
        , m_prefix_path(prefix_path)
        , m_channel_context(channel_context)
    {
        auto conda_meta_dir = m_prefix_path / "conda-meta";
        if (lexists(conda_meta_dir))
        {
            for (auto& p : fs::directory_iterator(conda_meta_dir))
            {
                if (util::ends_with(p.path().string(), ".json"))
                {
                    load_single_record(p.path());
                }
            }
        }
        // Load packages installed with pip
        load_site_packages();
    }

    void PrefixData::add_packages(const std::vector<specs::PackageInfo>& packages)
    {
        for (const auto& pkg : packages)
        {
            LOG_DEBUG << "Adding virtual package: " << pkg.name << "=" << pkg.version << "="
                      << pkg.build_string;
            m_package_records.insert({ pkg.name, std::move(pkg) });
        }
    }

    const PrefixData::package_map& PrefixData::records() const
    {
        return m_package_records;
    }

    const PrefixData::package_map& PrefixData::pip_records() const
    {
        return m_pip_package_records;
    }

    PrefixData::package_map PrefixData::all_pkg_mgr_records() const
    {
        PrefixData::package_map merged_records = m_package_records;
        // Note that if the same key (pkg name) is present in both `m_package_records` and
        // `m_pip_package_records`, the latter is not considered
        // (this may be modified to be completely independent in the future)
        merged_records.insert(m_pip_package_records.begin(), m_pip_package_records.end());

        return merged_records;
    }

    std::vector<specs::PackageInfo> PrefixData::sorted_records() const
    {
        // TODO add_pip_as_python_dependency

        auto dep_graph = util::DiGraph<const specs::PackageInfo*>();
        using node_id = typename decltype(dep_graph)::node_id;

        {
            auto name_to_node_id = std::unordered_map<std::string_view, node_id>();

            // Add all nodes
            for (const auto& [name, record] : records())
            {
                name_to_node_id[name] = dep_graph.add_node(&record);
            }
            // Add all inverse dependency edges.
            // Since there must be only one package with a given name, we assume that the dependency
            // version are matched properly and that only names must be checked.
            for (const auto& [to_id, record] : dep_graph.nodes())
            {
                for (const auto& dep : record->dependencies)
                {
                    // Creating a matchspec to parse the name (there may be a channel)
                    auto ms = specs::MatchSpec::parse(dep)
                                  .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                  .value();
                    // Ignoring unmatched dependencies, the environment could be broken
                    // or it could be a matchspec
                    const auto from_iter = name_to_node_id.find(ms.name().str());
                    if (from_iter != name_to_node_id.cend())
                    {
                        dep_graph.add_edge(from_iter->second, to_id);
                    }
                }
            }

            // Flip known problematic edges.
            // This is made to address cycles but there is no straightforward way to make
            // a generic cycle handler so we instead force flip the given edges
            static constexpr auto edges_to_flip = std::array{ std::pair{ "pip", "python" } };
            for (const auto& [from, to] : edges_to_flip)
            {
                const auto from_iter = name_to_node_id.find(from);
                const auto to_iter = name_to_node_id.find(to);
                const auto end_iter = name_to_node_id.cend();
                if ((from_iter != end_iter) && (to_iter != end_iter))
                {
                    const auto from_id = from_iter->second;
                    const auto to_id = to_iter->second;
                    if (dep_graph.has_edge(from_id, to_id))
                    {
                        dep_graph.remove_edge(from_id, to_id);
                        dep_graph.add_edge(to_id, from_id);  // safe if edge already exeists
                    }
                }
            }
        }

        auto sorted = std::vector<specs::PackageInfo>();
        sorted.reserve(dep_graph.number_of_nodes());
        util::topological_sort_for_each_node_id(
            dep_graph,
            [&](node_id id) { sorted.push_back(*dep_graph.node(id)); }
        );

        return sorted;
    }

    History& PrefixData::history()
    {
        return m_history;
    }

    const fs::u8path& PrefixData::path() const
    {
        return m_prefix_path;
    }

    void PrefixData::load_single_record(const fs::u8path& path)
    {
        LOG_INFO << "Loading single package record: " << path;
        auto infile = open_ifstream(path);
        nlohmann::json j;
        infile >> j;
        auto prec = j.get<specs::PackageInfo>();

        // Some versions of micromamba constructor generate repodata_record.json
        // and conda-meta json files with channel names while mamba expects
        // specs::PackageInfo channels to be platform urls. This fixes the issue described
        // in https://github.com/mamba-org/mamba/issues/2665

        auto channels = m_channel_context.make_channel(prec.channel);
        // If someone wrote multichannel names in repodata_record, we don't know which one is the
        // correct URL. This must never happen!
        assert(channels.size() == 1);
        using Credentials = specs::CondaURL::Credentials;
        prec.channel = channels.front().platform_url(prec.platform).str(Credentials::Remove);
        m_package_records.insert({ prec.name, std::move(prec) });
    }

    // Load python packages installed with pip in the site-packages of the prefix.
    void PrefixData::load_site_packages()
    {
        LOG_INFO << "Loading site packages";

        // Look for `pip` package and return if it doesn't exist
        auto python_pkg_record = m_package_records.find("pip");
        if (python_pkg_record == m_package_records.end())
        {
            LOG_DEBUG << "`pip` not found";
            return;
        }

        // Run `pip inspect`
        std::string out, err;

        const auto get_python_path = [&]
        { return util::which_in("python", util::get_path_dirs(m_prefix_path)).string(); };

        const auto args = std::array<std::string, 5>{ get_python_path(),
                                                      "-m",
                                                      "pip",
                                                      "inspect",
                                                      "--local" };

        const std::vector<std::pair<std::string, std::string>> env{ { "PYTHONIOENCODING", "utf-8" } };
        reproc::options run_options;
        run_options.env.extra = reproc::env{ env };

        LOG_TRACE << "Running command: "
                  << fmt::format("{}\n  env options:{}", fmt::join(args, " "), fmt::join(env, " "));

        auto [status, ec] = reproc::run(
            args,
            run_options,
            reproc::sink::string(out),
            reproc::sink::string(err)
        );

        if (ec)
        {
            const auto message = fmt::format(
                "failed to run python command :\n  error: {}\n  command ran: {}\n  env options:{}\n-> output:\n{}\n\n-> error output:{}",
                ec.message(),
                fmt::join(args, " "),
                fmt::join(env, " "),
                out,
                err
            );
            throw mamba_error{ message, mamba_error_code::internal_failure };
        }

        // Nothing installed with `pip`
        if (out.empty())
        {
            LOG_DEBUG << "Nothing installed with `pip`";
            return;
        }

        LOG_TRACE << "Parsing `pip inspect` output:\n" << out;
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(out);
        }
        catch (const std::exception& ec)
        {
            const auto message = fmt::format(
                "failed to parse python command output:\n  error: {}\n  command ran: {}\n  env options:{}\n-> output:\n{}\n\n-> error output:{}",
                ec.what(),
                fmt::join(args, " "),
                fmt::join(env, " "),
                out,
                err
            );
            throw mamba_error{ message, mamba_error_code::internal_failure };
        }

        if (j.contains("installed") && j["installed"].is_array())
        {
            for (const auto& package : j["installed"])
            {
                // Get the package metadata, if requested and installed with `pip`
                if (package.contains("requested") && package.contains("installer")
                    && package["requested"] == true && package["installer"] == "pip")
                {
                    if (package.contains("metadata"))
                    {
                        // NOTE As checking the presence of all used keys in the json object can be
                        // cumbersome and might affect the code readability, the elements where the
                        // check with `contains` is skipped are considered mandatory. If a bug is
                        // ever to occur in the future, checking the relevant key with `contains`
                        // should be introduced then.
                        auto prec = specs::PackageInfo(
                            package["metadata"]["name"],
                            package["metadata"]["version"],
                            "pypi_0",
                            "pypi"
                        );
                        // Set platform by concatenating `sys_platform` and `platform_machine` to
                        // have something equivalent to `conda-forge`
                        if (j.contains("environment"))
                        {
                            prec.platform = j["environment"]["sys_platform"].get<std::string>() + "-"
                                            + j["environment"]["platform_machine"].get<std::string>();
                        }
                        m_pip_package_records.insert({ prec.name, std::move(prec) });
                    }
                }
            }
        }
    }
}  // namespace mamba
