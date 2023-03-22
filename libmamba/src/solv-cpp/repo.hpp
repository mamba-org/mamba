// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_REPO_HPP
#define MAMBA_SOLV_REPO_HPP

#include <filesystem>
#include <string_view>
#include <utility>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/solvable.hpp"

typedef struct s_Repo Repo;

namespace mamba::solv
{

    class ObjRepoViewConst
    {
    public:

        explicit ObjRepoViewConst(const ::Repo* repo) noexcept;

        auto raw() const -> const ::Repo*;

        auto name() const -> std::string_view;
        auto n_solvables() const -> std::size_t;
        auto contains_solvable(SolvableId id) const -> bool;

        template <typename UnaryFunc>
        void for_each_solvable_id(UnaryFunc func) const;

        /**
         * Write repository information to file.
         *
         * @param solv_file A standard path with system encoding.
         */
        void write(std::filesystem::path solv_file) const;

    private:

        const ::Repo* m_repo = nullptr;
    };

    class ObjRepoView : public ObjRepoViewConst
    {
    public:

        explicit ObjRepoView(::Repo* repo) noexcept;

        auto raw() const -> ::Repo*;

        void clear(bool reuse_ids) const;

        /**
         * Read repository information from file.
         *
         * @param solv_file A standard path with system encoding.
         */
        void read(std::filesystem::path solv_file) const;

        auto add_solvable() const -> SolvableId;
    };
}

#include <solv/repo.h>

namespace mamba::solv
{
    /****************************************
     *  Implementation of ObjRepoViewConst  *
     ****************************************/

    template <typename UnaryFunc>
    void ObjRepoViewConst::for_each_solvable_id(UnaryFunc func) const
    {
        const ::Repo* const repo = raw();
        SolvableId id = 0;
        const ::Solvable* s = nullptr;
        FOR_REPO_SOLVABLES(repo, id, s)
        {
            func(id);
        }
    }
}
#endif
