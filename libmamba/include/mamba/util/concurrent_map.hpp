// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_CONCURRENT_MAP_HPP
#define MAMBA_UTIL_CONCURRENT_MAP_HPP

#include <array>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace mamba::util
{

    /**
     * A thread-safe map with stripe locking for concurrent access.
     *
     * Uses multiple mutexes (stripes) to allow concurrent operations on different keys.
     * Keys are distributed across stripes via hashing, so operations on different keys
     * typically lock different mutexes and can proceed in parallel.
     *
     * @tparam Key Key type (must be hashable)
     * @tparam Value Value type
     * @tparam Hash Hash function for key distribution across stripes
     * @tparam NumStripes Number of lock stripes (default 64)
     */
    template <typename Key, typename Value, typename Hash = std::hash<Key>, std::size_t NumStripes = 64>
    class concurrent_map
    {
    public:

        using key_type = Key;
        using mapped_type = Value;
        using value_type = std::pair<const Key, Value>;
        using map_type = std::unordered_map<Key, Value, Hash>;

        concurrent_map() = default;

        /** Insert or assign a key-value pair. Thread-safe. */
        void insert_or_assign(const Key& key, Value value)
        {
            auto& s = stripe(key);
            std::lock_guard lock(s.mutex);
            s.map.insert_or_assign(key, std::move(value));
        }

        /** Find a key. Returns value if found, std::nullopt otherwise. Thread-safe. */
        auto find(const Key& key) const -> std::optional<Value>
        {
            auto& s = stripe(key);
            std::shared_lock lock(s.mutex);
            auto it = s.map.find(key);
            if (it == s.map.end())
            {
                return std::nullopt;
            }
            return it->second;
        }

        /** Check if key exists. Thread-safe. */
        auto contains(const Key& key) const -> bool
        {
            auto& s = stripe(key);
            std::shared_lock lock(s.mutex);
            return s.map.find(key) != s.map.end();
        }

        /**
         * Apply a function to each key-value pair.
         * Locks all stripes for the duration. Use for iteration/snapshot.
         * Thread-safe but blocks all other operations during the call.
         */
        template <typename Func>
        void for_each(Func&& func) const
        {
            for (auto& s : m_stripes)
            {
                std::shared_lock lock(s.mutex);
                for (const auto& [k, v] : s.map)
                {
                    func(k, v);
                }
            }
        }

        /**
         * Copy all entries into a standard map.
         * Useful when a snapshot is needed for further processing.
         */
        auto copy() const -> map_type
        {
            map_type result;
            for_each([&result](const Key& k, const Value& v) { result[k] = v; });
            return result;
        }

        /** Copy constructor. */
        concurrent_map(const concurrent_map& other)
        {
            other.for_each([this](const Key& k, const Value& v) { insert_or_assign(k, v); });
        }

        /** Copy assignment. */
        auto operator=(const concurrent_map& other) -> concurrent_map&
        {
            if (this != &other)
            {
                clear();
                other.for_each([this](const Key& k, const Value& v) { insert_or_assign(k, v); });
            }
            return *this;
        }

        /** Move constructor. */
        concurrent_map(concurrent_map&&) noexcept = default;

        /** Move assignment. */
        auto operator=(concurrent_map&&) noexcept -> concurrent_map& = default;

        /** Remove all entries. */
        void clear()
        {
            for (auto& s : m_stripes)
            {
                std::lock_guard lock(s.mutex);
                s.map.clear();
            }
        }

    private:

        struct stripe_type
        {
            mutable std::shared_mutex mutex;
            map_type map;
        };

        mutable std::array<stripe_type, NumStripes> m_stripes;

        auto stripe(const Key& key) -> stripe_type&
        {
            return m_stripes[Hash{}(key) % NumStripes];
        }

        auto stripe(const Key& key) const -> stripe_type&
        {
            return m_stripes[Hash{}(key) % NumStripes];
        }
    };

}  // namespace mamba::util

#endif
