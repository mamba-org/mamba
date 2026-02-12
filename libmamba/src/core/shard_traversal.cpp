// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <set>
#include <vector>

#include <fmt/format.h>

#include "mamba/core/context.hpp"
#include "mamba/core/logging.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/shard_traversal.hpp"
#include "mamba/specs/match_spec.hpp"

namespace mamba
{
    bool NodeId::operator==(const NodeId& other) const
    {
        return package == other.package && channel == other.channel && shard_url == other.shard_url;
    }

    bool NodeId::operator<(const NodeId& other) const
    {
        if (package != other.package)
        {
            return package < other.package;
        }
        if (channel != other.channel)
        {
            return channel < other.channel;
        }
        return shard_url < other.shard_url;
    }

    NodeId Node::to_id() const
    {
        return NodeId{ package, channel, shard_url };
    }

    namespace
    {
        void add_names_from_specs(const std::vector<std::string>& specs, std::set<std::string>& names)
        {
            for (const auto& spec : specs)
            {
                auto parsed = specs::MatchSpec::parse(spec);
                if (parsed)
                {
                    auto name = parsed->name().to_string();
                    if (!name.empty() && !parsed->name().is_free())
                    {
                        names.insert(std::move(name));
                    }
                }
            }
        }

        void add_names_from_record(const ShardPackageRecord& record, std::set<std::string>& names)
        {
            add_names_from_specs(record.depends, names);
            add_names_from_specs(record.constrains, names);
        }

        std::vector<std::string> extract_dependencies_impl(const ShardDict& shard)
        {
            std::set<std::string> names;
            for (const auto& [_, record] : shard.packages)
            {
                add_names_from_record(record, names);
            }
            for (const auto& [_, record] : shard.conda_packages)
            {
                add_names_from_record(record, names);
            }
            return std::vector<std::string>(names.begin(), names.end());
        }
    }  // namespace

    std::vector<std::string> shard_mentioned_packages(const ShardDict& shard)
    {
        return extract_dependencies_impl(shard);
    }

    /******************
     * RepodataSubset *
     ******************/

    RepodataSubset::RepodataSubset(ShardsPtrVector shards)
        : m_shards(std::move(shards))
    {
        for (const auto& s : m_shards)
        {
            m_shards_by_url[s->url()] = s;
        }
    }

    void RepodataSubset::reachable(
        const std::vector<std::string>& root_packages,
        const std::string& strategy,
        std::optional<std::reference_wrapper<const std::set<std::string>>> root_shards
    )
    {
        if (root_packages.empty())
        {
            return;
        }
        if (Context::can_report_status())
        {
            Console::instance().print(
                fmt::format("{:<85} {:>20}", "Fetching and Parsing Packages' Shards", "⧖ Starting")
            );
        }
        if (strategy == "bfs")
        {
            reachable_bfs(root_packages, root_shards);
        }
        else
        {
            reachable_pipelined(root_packages, root_shards);
        }
        if (Context::can_report_status())
        {
            Console::instance().print(
                fmt::format("{:<85} {:>20}", "Fetching and Parsing Packages' Shards", "✔ Done")
            );
        }
    }

    const NodeMap& RepodataSubset::nodes() const
    {
        return m_nodes;
    }

    const ShardsPtrVector& RepodataSubset::shards() const
    {
        return m_shards;
    }

    void RepodataSubset::init_pending_with_roots(
        const std::vector<std::string>& root_packages,
        std::optional<std::reference_wrapper<const std::set<std::string>>> root_shards,
        std::vector<NodeId>& pending
    )
    {
        for (const auto& pkg : root_packages)
        {
            for (const auto& [url, shards_ptr] : m_shards_by_url)
            {
                if (!shards_ptr->contains(pkg))
                {
                    continue;
                }
                std::string shard_url = shards_ptr->shard_url(pkg);
                if (root_shards.has_value() && !root_shards->get().contains(shard_url))
                {
                    continue;
                }
                NodeId id{ pkg, url, shard_url };
                if (!m_nodes.contains(id))
                {
                    m_nodes[id] = Node{ 0, pkg, url, shard_url, false };
                    pending.push_back(id);
                }
            }
        }
    }

    void RepodataSubset::fetch_missing_shards_for_batch(const std::vector<NodeId>& batch)
    {
        std::map<std::string, std::vector<std::string>> to_fetch_by_channel;
        for (const auto& id : batch)
        {
            auto it = m_shards_by_url.find(id.channel);
            if (it == m_shards_by_url.end())
            {
                continue;
            }
            auto& shards_ptr = it->second;
            if (!shards_ptr->is_shard_present(id.package))
            {
                to_fetch_by_channel[id.channel].push_back(id.package);
            }
        }
        for (auto& [channel, to_fetch] : to_fetch_by_channel)
        {
            std::sort(to_fetch.begin(), to_fetch.end());
            to_fetch.erase(std::unique(to_fetch.begin(), to_fetch.end()), to_fetch.end());
            if (!to_fetch.empty())
            {
                auto it = m_shards_by_url.find(channel);
                if (it != m_shards_by_url.end())
                {
                    auto result = it->second->fetch_shards(to_fetch);
                    if (result)
                    {
                        for (const auto& [pkg, shard] : result.value())
                        {
                            it->second->process_fetched_shard(pkg, shard);
                        }
                    }
                }
            }
        }
    }

    void
    RepodataSubset::process_bfs_batch(const std::vector<NodeId>& batch, std::vector<NodeId>& pending)
    {
        for (const auto& id : batch)
        {
            auto& node = m_nodes[id];
            node.visited = true;
            for (const auto& neighbor_id : neighbors(id))
            {
                if (!m_nodes.contains(neighbor_id))
                {
                    m_nodes[neighbor_id] = Node{
                        node.distance + 1,
                        neighbor_id.package,
                        neighbor_id.channel,
                        neighbor_id.shard_url,
                        false,
                    };
                    pending.push_back(neighbor_id);
                }
            }
        }
    }

    void RepodataSubset::reachable_bfs(
        const std::vector<std::string>& root_packages,
        std::optional<std::reference_wrapper<const std::set<std::string>>> root_shards
    )
    {
        std::vector<NodeId> pending;
        init_pending_with_roots(root_packages, root_shards, pending);

        while (!pending.empty())
        {
            std::vector<NodeId> batch;
            std::swap(batch, pending);
            fetch_missing_shards_for_batch(batch);
            process_bfs_batch(batch, pending);
        }
    }

    void RepodataSubset::reachable_pipelined(
        const std::vector<std::string>& root_packages,
        std::optional<std::reference_wrapper<const std::set<std::string>>> root_shards
    )
    {
        std::vector<NodeId> pending;
        init_pending_with_roots(root_packages, root_shards, pending);
        drain_pending(pending);
    }

    void RepodataSubset::visit_node(const NodeId& node_id, std::vector<NodeId>& pending)
    {
        auto it = m_shards_by_url.find(node_id.channel);
        if (it == m_shards_by_url.end())
        {
            return;
        }
        auto& shards_ptr = it->second;

        if (!shards_ptr->is_shard_present(node_id.package))
        {
            auto result = shards_ptr->fetch_shards({ node_id.package });
            if (!result)
            {
                LOG_WARNING << "Failed to fetch shard for " << node_id.package << ": "
                            << result.error().what();
                return;
            }
            auto shard_it = result.value().find(node_id.package);
            if (shard_it != result.value().end())
            {
                shards_ptr->process_fetched_shard(node_id.package, shard_it->second);
            }
        }

        auto& node = m_nodes[node_id];
        node.visited = true;

        ShardDict shard;
        try
        {
            shard = shards_ptr->visit_package(node_id.package);
        }
        catch (const std::exception& e)
        {
            LOG_WARNING << "Could not visit shard for " << node_id.package << ": " << e.what();
            return;
        }

        for (const auto& dep : extract_dependencies_impl(shard))
        {
            for (const auto& [url, dep_shards] : m_shards_by_url)
            {
                if (!dep_shards->contains(dep))
                {
                    continue;
                }
                NodeId neighbor_id{ dep, url, dep_shards->shard_url(dep) };
                if (!m_nodes.contains(neighbor_id))
                {
                    m_nodes[neighbor_id] = Node{
                        node.distance + 1, dep, url, neighbor_id.shard_url, false,
                    };
                    pending.push_back(neighbor_id);
                }
            }
        }
    }

    void RepodataSubset::drain_pending(std::vector<NodeId>& pending)
    {
        while (!pending.empty())
        {
            NodeId id = pending.back();
            pending.pop_back();
            if (!m_nodes[id].visited)
            {
                visit_node(id, pending);
            }
        }
    }

    std::vector<NodeId> RepodataSubset::neighbors(const NodeId& node_id)
    {
        auto it = m_shards_by_url.find(node_id.channel);
        if (it == m_shards_by_url.end())
        {
            return {};
        }
        auto& shards_ptr = it->second;

        if (!shards_ptr->is_shard_present(node_id.package))
        {
            return {};
        }

        ShardDict shard;
        try
        {
            shard = shards_ptr->visit_package(node_id.package);
        }
        catch (const std::exception&)
        {
            return {};
        }

        std::vector<NodeId> result;
        for (const auto& dep : extract_dependencies_impl(shard))
        {
            for (const auto& [url, dep_shards] : m_shards_by_url)
            {
                if (dep_shards->contains(dep))
                {
                    result.push_back({ dep, url, dep_shards->shard_url(dep) });
                }
            }
        }
        return result;
    }

}
