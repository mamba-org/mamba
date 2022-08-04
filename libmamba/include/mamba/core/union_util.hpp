// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UNION_UTIL_HPP
#define MAMBA_CORE_UNION_UTIL_HPP

#include <map>
#include <utility>
#include <vector>

namespace mamba
{
    template<class T>
    class Union
    {
    public:
        std::unordered_map<T, T> parent;
        std::unordered_map<T, T> rank;

        void add(T node);
        void connect(T node_u, T node_v);

        const T root(T node);
    };


    template<class T>
    inline void Union<T>::add(T node)
    {
        parent[node] = node;
        rank[node] = node;
    }

    template<class T>
    inline void Union<T>::connect(T node_u, T node_v)
    {
        node_u=root(node_u);
        node_v=root(node_v);
        if(node_u == node_v)
        {
            return;
        }
        if(rank[node_u] < rank[node_v])
        {
            std::swap(node_u, node_v);
        }
        parent[node_v] = node_u;
        if(rank[node_u] == rank[node_v])
        {
            rank[node_u]++;
        }
    }

    template<class T>
    inline auto Union<T>::root(T node) -> const T
    {
        if(node == parent[node])
        {
            return node;
        }

        return parent[node] = root(parent[node]);
    }

} // namespace mamba

#endif  // MAMBA_CORE_UNION_UTIL_HPP
