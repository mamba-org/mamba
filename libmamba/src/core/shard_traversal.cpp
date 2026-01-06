// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <unordered_set>

#include "mamba/core/context.hpp"
#include "mamba/core/logging.hpp"
#include "mamba/core/shard_loader.hpp"
#include "mamba/core/shard_traversal.hpp"
#include "mamba/core/shard_types.hpp"
#include "mamba/specs/error.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/environment.hpp"

namespace mamba
{
    /******************
     * NodeId         *
     ******************/

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

    /******************
     * Node           *
     ******************/

    NodeId Node::to_id() const
    {
        return NodeId{ package, channel, shard_url };
    }

    /******************
     * Helper functions *
     ******************/

    auto shard_mentioned_packages(const ShardDict& shard) -> std::vector<std::string>
    {
        std::unordered_set<std::string> unique_names;

        // Process all packages in the shard
        auto process_deps = [&unique_names](const std::vector<std::string>& depends)
        {
            for (const auto& dep_spec : depends)
            {
                // Parse dependency spec to get package name
                auto parsed = specs::MatchSpec::parse(dep_spec);
                if (parsed.has_value())
                {
                    std::string name = parsed.value().name().to_string();
                    if (!name.empty())
                    {
                        unique_names.insert(name);
                    }
                }
                else
                {
                    // Log parse error but continue
                    LOG_DEBUG << "Failed to parse dependency spec: " << dep_spec;
                }
            }
        };

        for (const auto& [filename, record] : shard.packages)
        {
            process_deps(record.depends);
        }

        for (const auto& [filename, record] : shard.conda_packages)
        {
            process_deps(record.depends);
        }

        return std::vector<std::string>(unique_names.begin(), unique_names.end());
    }

    /******************
     * RepodataSubset *
     ******************/

    RepodataSubset::RepodataSubset(std::vector<std::shared_ptr<ShardBase>> shardlikes)
        : m_shardlikes(std::move(shardlikes))
    {
    }

    auto RepodataSubset::nodes() const -> const std::map<NodeId, Node>&
    {
        return m_nodes;
    }

    auto RepodataSubset::shardlikes() const -> const std::vector<std::shared_ptr<ShardBase>>&
    {
        return m_shardlikes;
    }

    auto RepodataSubset::extract_dependencies(const ShardDict& shard) -> std::vector<std::string>
    {
        return shard_mentioned_packages(shard);
    }

    auto RepodataSubset::neighbors(const Node& node) -> std::vector<Node>
    {
        std::vector<Node> result;
        std::set<std::string> discovered;

        // Find the shardlike for this node's channel
        std::shared_ptr<ShardBase> shardlike = nullptr;
        for (const auto& sl : m_shardlikes)
        {
            if (sl->url() == node.channel)
            {
                shardlike = sl;
                break;
            }
        }

        if (!shardlike)
        {
            return result;
        }

        // Fetch the shard if not already loaded
        if (!shardlike->shard_loaded(node.package))
        {
            auto fetch_result = shardlike->fetch_shard(node.package);
            if (!fetch_result.has_value())
            {
                LOG_WARNING << "Failed to fetch shard for package " << node.package;
                return result;
            }
        }

        // Get the shard and extract dependencies
        ShardDict shard = shardlike->visit_package(node.package);
        std::vector<std::string> mentioned_packages = extract_dependencies(shard);

        // Create nodes for dependencies
        for (const auto& package_name : mentioned_packages)
        {
            if (discovered.find(package_name) != discovered.end())
            {
                continue;
            }
            discovered.insert(package_name);

            // Check if package exists in any shardlike
            for (const auto& sl : m_shardlikes)
            {
                if (sl->contains(package_name))
                {
                    NodeId node_id{ package_name, sl->url(), sl->shard_url(package_name) };
                    if (m_nodes.find(node_id) == m_nodes.end())
                    {
                        Node new_node;
                        new_node.distance = node.distance + 1;
                        new_node.package = package_name;
                        new_node.channel = sl->url();
                        new_node.shard_url = sl->shard_url(package_name);
                        new_node.visited = false;
                        m_nodes[node_id] = new_node;
                        result.push_back(new_node);
                    }
                }
            }
        }

        return result;
    }

    auto
    RepodataSubset::visit_node(const Node& parent_node, const std::vector<std::string>& mentioned_packages)
        -> std::vector<NodeId>
    {
        std::vector<NodeId> pending;

        for (const auto& package_name : mentioned_packages)
        {
            // Broadcast mentioned packages across all channels
            for (const auto& shardlike : m_shardlikes)
            {
                if (shardlike->contains(package_name))
                {
                    NodeId new_node_id{ package_name,
                                        shardlike->url(),
                                        shardlike->shard_url(package_name) };
                    if (m_nodes.find(new_node_id) == m_nodes.end())
                    {
                        Node new_node;
                        new_node.distance = parent_node.distance + 1;
                        new_node.package = new_node_id.package;
                        new_node.channel = new_node_id.channel;
                        new_node.shard_url = new_node_id.shard_url;
                        new_node.visited = false;
                        m_nodes[new_node_id] = new_node;
                        pending.push_back(new_node_id);
                    }
                }
            }
        }

        return pending;
    }

    auto RepodataSubset::drain_pending(
        std::set<NodeId>& pending,
        std::map<std::string, std::shared_ptr<ShardBase>>& shardlikes_by_url
    ) -> std::pair<std::vector<std::pair<NodeId, ShardDict>>, std::vector<NodeId>>
    {
        std::vector<std::pair<NodeId, ShardDict>> have;
        std::vector<NodeId> need;

        for (const auto& node_id : pending)
        {
            auto it = shardlikes_by_url.find(node_id.channel);
            if (it == shardlikes_by_url.end())
            {
                continue;
            }

            auto& shardlike = it->second;
            if (shardlike->shard_loaded(node_id.package))
            {
                // Shard already in memory
                ShardDict shard = shardlike->visit_package(node_id.package);
                have.emplace_back(node_id, shard);
            }
            else
            {
                // Need to fetch
                auto node_it = m_nodes.find(node_id);
                if (node_it != m_nodes.end() && node_it->second.visited)
                {
                    continue;  // Skip already visited
                }
                need.push_back(node_id);
            }
        }

        pending.clear();
        return { have, need };
    }

    auto
    RepodataSubset::reachable(const std::vector<std::string>& root_packages, const std::string& strategy)
        -> expected_t<void>
    {
        if (strategy == "bfs")
        {
            return reachable_bfs(root_packages);
        }
        else if (strategy == "pipelined")
        {
            return reachable_pipelined(root_packages);
        }
        else
        {
            return make_unexpected(
                "Unknown traversal strategy: " + strategy,
                mamba_error_code::incorrect_usage
            );
        }
    }

    auto RepodataSubset::reachable_bfs(const std::vector<std::string>& root_packages)
        -> expected_t<void>
    {
        // Initialize root nodes
        Node parent_node;
        parent_node.distance = 0;
        auto root_node_ids = visit_node(parent_node, root_packages);

        std::deque<NodeId> node_queue(root_node_ids.begin(), root_node_ids.end());

        // Build map of shardlikes by URL
        std::map<std::string, std::shared_ptr<ShardBase>> shardlikes_by_url;
        for (const auto& sl : m_shardlikes)
        {
            shardlikes_by_url[sl->url()] = sl;
        }

        // Separate sharded from non-sharded
        std::vector<std::shared_ptr<Shards>> sharded;
        for (const auto& sl : m_shardlikes)
        {
            if (auto shards = std::dynamic_pointer_cast<Shards>(sl))
            {
                sharded.push_back(shards);
            }
        }

        while (!node_queue.empty())
        {
            // Batch fetch all nodes at current level
            std::set<NodeId> to_retrieve;
            std::size_t level_size = node_queue.size();
            for (std::size_t i = 0; i < level_size; ++i)
            {
                NodeId node_id = node_queue.front();
                node_queue.pop_front();

                auto node_it = m_nodes.find(node_id);
                if (node_it == m_nodes.end() || node_it->second.visited)
                {
                    continue;
                }

                node_it->second.visited = true;
                to_retrieve.insert(node_id);
            }

            // Fetch shards for this level
            if (!to_retrieve.empty())
            {
                std::vector<std::string> packages_to_fetch;
                for (const auto& node_id : to_retrieve)
                {
                    packages_to_fetch.push_back(node_id.package);
                }

                // Batch fetch from cache and network
                // TODO: Implement batch_retrieve_from_cache and batch_retrieve_from_network
                for (const auto& node_id : to_retrieve)
                {
                    auto it = shardlikes_by_url.find(node_id.channel);
                    if (it != shardlikes_by_url.end())
                    {
                        auto fetch_result = it->second->fetch_shard(node_id.package);
                        if (fetch_result.has_value())
                        {
                            it->second->visit_shard(node_id.package, fetch_result.value());
                        }
                    }
                }

                // Discover neighbors
                for (const auto& node_id : to_retrieve)
                {
                    auto node_it = m_nodes.find(node_id);
                    if (node_it != m_nodes.end())
                    {
                        auto& node = node_it->second;
                        auto neighbors_list = neighbors(node);
                        for (const auto& neighbor : neighbors_list)
                        {
                            if (!neighbor.visited)
                            {
                                node_queue.push_back(neighbor.to_id());
                            }
                        }
                    }
                }
            }
        }

        return expected_t<void>();
    }

    namespace
    {
        /**
         * Thread-safe queue for passing NodeIds between threads.
         */
        template <typename T>
        class ThreadSafeQueue
        {
        public:

            void push(const T& item)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_queue.push(item);
                m_condition.notify_one();
            }

            void push(T&& item)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_queue.push(std::move(item));
                m_condition.notify_one();
            }

            bool try_pop(T& item, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_condition.wait_for(lock, timeout, [this] { return !m_queue.empty() || m_done; }))
                {
                    if (m_done && m_queue.empty())
                    {
                        return false;
                    }
                    item = std::move(m_queue.front());
                    m_queue.pop();
                    return true;
                }
                return false;
            }

            void set_done()
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_done = true;
                m_condition.notify_all();
            }

            bool empty() const
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                return m_queue.empty();
            }

        private:

            mutable std::mutex m_mutex;
            std::queue<T> m_queue;
            std::condition_variable m_condition;
            bool m_done = false;
        };

        /**
         * Network worker thread function.
         * Fetches shards from network that are received on in_queue.
         */
        void network_fetch_thread(
            ThreadSafeQueue<std::vector<NodeId>>& network_in_queue,
            ThreadSafeQueue<std::vector<std::pair<NodeId, ShardDict>>>& shard_out_queue,
            const std::vector<std::shared_ptr<ShardBase>>& shardlikes
        )
        {
            std::map<std::string, std::shared_ptr<ShardBase>> shardlikes_by_url;
            for (const auto& sl : shardlikes)
            {
                shardlikes_by_url[sl->url()] = sl;
            }

            std::vector<NodeId> batch;
            while (network_in_queue.try_pop(batch, std::chrono::milliseconds(5000)))
            {
                if (batch.empty())
                {
                    continue;
                }

                // Group by channel for batch fetching
                std::map<std::string, std::vector<std::string>> packages_by_channel;
                for (const auto& node_id : batch)
                {
                    packages_by_channel[node_id.channel].push_back(node_id.package);
                }

                // Fetch shards from each channel
                for (const auto& [channel_url, packages] : packages_by_channel)
                {
                    auto it = shardlikes_by_url.find(channel_url);
                    if (it == shardlikes_by_url.end())
                    {
                        continue;
                    }

                    auto fetch_result = it->second->fetch_shards(packages);
                    if (!fetch_result.has_value())
                    {
                        LOG_WARNING << "Failed to fetch shards for channel " << channel_url;
                        continue;
                    }

                    // Send fetched shards
                    std::vector<std::pair<NodeId, ShardDict>> fetched;
                    for (const auto& package : packages)
                    {
                        auto shard_it = fetch_result.value().find(package);
                        if (shard_it != fetch_result.value().end())
                        {
                            NodeId node_id{ package, channel_url, it->second->shard_url(package) };
                            fetched.emplace_back(node_id, shard_it->second);
                        }
                    }

                    if (!fetched.empty())
                    {
                        shard_out_queue.push(std::move(fetched));
                    }
                }
            }
        }

        /**
         * Offline worker thread function.
         * For offline mode, returns empty shards for cache misses.
         */
        void offline_nofetch_thread(
            ThreadSafeQueue<std::vector<NodeId>>& network_in_queue,
            ThreadSafeQueue<std::vector<std::pair<NodeId, ShardDict>>>& shard_out_queue,
            const std::vector<std::shared_ptr<ShardBase>>& /* shardlikes */
        )
        {
            std::vector<NodeId> batch;
            while (network_in_queue.try_pop(batch, std::chrono::milliseconds(5000)))
            {
                if (batch.empty())
                {
                    continue;
                }

                // Return empty shards
                std::vector<std::pair<NodeId, ShardDict>> empty_shards;
                for (const auto& node_id : batch)
                {
                    ShardDict empty_shard;
                    empty_shards.emplace_back(node_id, empty_shard);
                }

                shard_out_queue.push(std::move(empty_shards));
            }
        }
    }

    auto RepodataSubset::reachable_pipelined(const std::vector<std::string>& root_packages)
        -> expected_t<void>
    {
        // Get offline mode from context (we'll need to pass this in the future)
        // For now, assume online mode
        bool offline = false;

        // Create queues (no cache thread for now - all shards fetched from network)
        ThreadSafeQueue<std::vector<std::pair<NodeId, ShardDict>>> shard_out_queue;
        ThreadSafeQueue<std::vector<NodeId>> network_in_queue;

        // Start network worker thread
        std::thread network_thread(
            offline ? offline_nofetch_thread : network_fetch_thread,
            std::ref(network_in_queue),
            std::ref(shard_out_queue),
            std::cref(m_shardlikes)
        );

        // Build map of shardlikes by URL
        std::map<std::string, std::shared_ptr<ShardBase>> shardlikes_by_url;
        for (const auto& sl : m_shardlikes)
        {
            shardlikes_by_url[sl->url()] = sl;
        }

        // Initialize root nodes
        m_nodes.clear();
        Node parent_node;
        parent_node.distance = 0;
        auto root_node_ids = visit_node(parent_node, root_packages);
        std::set<NodeId> pending(root_node_ids.begin(), root_node_ids.end());

        std::set<NodeId> in_flight;
        int timeouts = 0;
        constexpr int MAX_TIMEOUTS = 10;

        // Main traversal loop
        while (true)
        {
            // Drain pending nodes
            auto [have, need] = drain_pending(pending, shardlikes_by_url);

            // Process nodes we already have
            for (const auto& [node_id, shard] : have)
            {
                in_flight.insert(node_id);
                pending.erase(node_id);  // Remove from pending since we're processing it
                auto node_it = m_nodes.find(node_id);
                if (node_it != m_nodes.end())
                {
                    auto& node = node_it->second;
                    auto shardlike = shardlikes_by_url[node_id.channel];
                    shardlike->visit_shard(node_id.package, shard);

                    // Discover new dependencies
                    auto mentioned = shard_mentioned_packages(shard);
                    auto new_pending = visit_node(node, mentioned);
                    for (const auto& new_node_id : new_pending)
                    {
                        pending.insert(new_node_id);
                    }
                }
                in_flight.erase(node_id);  // Remove from in_flight after processing
            }

            // Request network fetch for nodes we need
            if (!need.empty())
            {
                in_flight.insert(need.begin(), need.end());
                // Remove from pending since we're requesting them
                for (const auto& node_id : need)
                {
                    pending.erase(node_id);
                }
                network_in_queue.push(std::vector<NodeId>(need.begin(), need.end()));
            }

            // Check if we're done
            if (in_flight.empty() && pending.empty())
            {
                LOG_DEBUG << "All shards have finished processing.";
                break;
            }

            // Wait for shards from queue
            std::vector<std::pair<NodeId, ShardDict>> new_shards;
            if (shard_out_queue.try_pop(new_shards, std::chrono::milliseconds(1000)))
            {
                timeouts = 0;  // Reset timeout counter on success

                for (const auto& [node_id, shard] : new_shards)
                {
                    in_flight.erase(node_id);

                    auto node_it = m_nodes.find(node_id);
                    if (node_it == m_nodes.end())
                    {
                        continue;
                    }

                    auto& node = node_it->second;
                    auto shardlike = shardlikes_by_url[node_id.channel];
                    shardlike->visit_shard(node_id.package, shard);

                    // Discover new dependencies
                    auto mentioned = shard_mentioned_packages(shard);
                    auto new_pending = visit_node(node, mentioned);
                    for (const auto& new_node_id : new_pending)
                    {
                        pending.insert(new_node_id);
                    }
                }
            }
            else
            {
                timeouts++;
                LOG_DEBUG << "Shard timeout " << timeouts;
                if (timeouts > MAX_TIMEOUTS)
                {
                    // Signal thread to stop
                    network_in_queue.set_done();

                    // Join thread
                    if (network_thread.joinable())
                    {
                        network_thread.join();
                    }

                    return make_unexpected(
                        "Timeout while fetching repodata shards",
                        mamba_error_code::repodata_not_loaded
                    );
                }
            }
        }

        // Signal thread to stop
        network_in_queue.set_done();

        // Join thread
        if (network_thread.joinable())
        {
            network_thread.join();
        }

        return expected_t<void>();
    }
}
