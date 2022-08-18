// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UNION_FIND_HPP
#define MAMBA_CORE_UNION_FIND_HPP

#include <map>
#include <utility>
#include <vector>
#include <unordered_map>

#include "mamba/core/output.hpp"


namespace mamba
{
    template <class T>
    class UnionFind
    {
    public:
        using unions_info = std::unordered_map<T, std::vector<T>>;

        std::unordered_map<T, T> parent;
        std::unordered_map<T, int> rank;

        void add(T node);
        void connect(T node_u, T node_v);

        const T root(T node);
        unions_info get_unions();
    };


    template <class T>
    inline void UnionFind<T>::add(T node)
    {
        parent[node] = node;
        rank[node] = 0;
    }

    template <class T>
    inline void UnionFind<T>::connect(T node_u, T node_v)
    {
        T u = root(node_u);
        T v = root(node_v);
        if (u == v)
        {
            return;
        }
        if (rank[u] < rank[v])
        {
            std::swap(u, v);
        }
        parent[v] = u;
        if (rank[u] == rank[v])
        {
            rank[u]++;
        }
    }

    template <class T>
    inline auto UnionFind<T>::get_unions() -> unions_info
    {
        unions_info groups;
        for (const auto& entry : parent)
        {
            groups[root(entry.first)].push_back(entry.first);
        }
        return groups;
    }


    template <class T>
    inline auto UnionFind<T>::root(T node) -> const T
    {
        if (node == parent[node])
        {
            return node;
        }

        return parent[node] = root(parent[node]);
    }

}  // namespace mamba

#endif  // MAMBA_CORE_UNION_FIND_HPP
