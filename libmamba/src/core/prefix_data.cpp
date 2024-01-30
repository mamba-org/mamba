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

#include "mamba/core/channel_context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/conda_url.hpp"
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
        load();
    }

    void PrefixData::load()
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
                for (const auto& dep : record->depends)
                {
                    // Creating a matchspec to parse the name (there may be a channel)
                    auto ms = specs::MatchSpec::parse(dep);
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
            // This is made to adress cycles but there is no straightforward way to make
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
        // correct URL. This is must never happen!
        assert(channels.size() == 1);
        using Credentials = specs::CondaURL::Credentials;
        prec.channel = channels.front().platform_url(prec.subdir).str(Credentials::Remove);
        m_package_records.insert({ prec.name, std::move(prec) });
    }

    // Load python packages installed with pip in the site-packages of the prefix.
    void PrefixData::load_site_packages()
    {
        LOG_INFO << "Loading site packages";

        // Look for "pip" package and return if it doesn't exist
        auto python_pkg_record = m_package_records.find("pip");
        if (python_pkg_record == m_package_records.end())
        {
            return;
        }

        // Run `pip freeze`
        std::string out, err;
        std::vector<std::string> args = { "pip", "freeze", "-l" };
        auto [status, ec] = reproc::run(
            args,
            reproc::options{},
            reproc::sink::string(out),
            reproc::sink::string(err)
        );
        if (ec)
        {
            throw std::runtime_error(ec.message());
        }

        // Nothing installed with `pip`
        if (out.empty())
        {
            return;
        }

        auto pkgs_info_list = mamba::util::split(mamba::util::strip(out), "\n");
        for (auto& pkg_info_line : pkgs_info_list)
        {
            if (pkg_info_line.find("==") != std::string::npos)
            {
                auto pkg_info = mamba::util::split(mamba::util::strip(pkg_info_line), "==");
                auto prec = specs::PackageInfo(pkg_info[0], pkg_info[1], "pypi_0", "pypi");
                m_package_records.insert({ prec.name, std::move(prec) });
            }
        }
    }

}  // namespace mamba
