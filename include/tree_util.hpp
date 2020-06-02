#ifndef MAMBA_TREE_UTIL_HPP
#define MAMBA_TREE_UTIL_HPP

#include <vector>

namespace mamba
{

    /*************
     * tree_node *
     *************/

    template <class T>
    struct tree_node
    {
        using self_type = tree_node<T>;
        using value_type = T;
        using children_list = std::vector<self_type>;

        explicit tree_node(const value_type& value);
        explicit tree_node(value_type&& value);

        void add_child(const value_type& value);
        void add_child(value_type&& value);

        void add_child(const self_type& node);
        void add_child(self_type&& node);

        value_type m_value;
        children_list m_children;
    };

    /****************************
     * tree_node implementation *
     ****************************/

    template <class T>
    inline tree_node<T>::tree_node(const value_type& value)
        : m_value(value)
        , m_children()
    {
    }

    template <class T>
    inline tree_node<T>::tree_node(value_type&& value)
        : m_value(std::move(value))
        , m_children()
    {
    }

    template <class T>
    inline void tree_node<T>::add_child(const value_type& value)
    {
        m_children.emplace_back(value);
    }

    template <class T>
    inline void tree_node<T>::add_child(value_type&& value)
    {
        m_children.emplace_back(std::move(value));
    }

    template <class T>
    inline void tree_node<T>::add_child(const self_type& node)
    {
        m_children.push_back(node);
    }

    template <class T>
    inline void tree_node<T>::add_child(self_type&& node)
    {
        m_children.push_back(std::move(node));
    }
}

#endif

