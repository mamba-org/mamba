// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_REPO_HPP
#define MAMBA_SOLV_REPO_HPP

#include <filesystem>
#include <optional>
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

        auto id() const -> RepoId;

        auto name() const -> std::string_view;
        auto url() const -> std::string_view;
        auto channel() const -> std::string_view;
        auto subdir() const -> std::string_view;

        auto n_solvables() const -> std::size_t;
        auto has_solvable(SolvableId id) const -> bool;
        auto get_solvable(SolvableId id) const -> std::optional<ObjSolvableViewConst>;

        template <typename UnaryFunc>
        void for_each_solvable_id(UnaryFunc func) const;
        template <typename UnaryFunc>
        void for_each_solvable(UnaryFunc func) const;

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

        using raw_str_view = const char*;

        explicit ObjRepoView(::Repo* repo) noexcept;

        auto raw() const -> ::Repo*;

        /** The following attributes need a call to @ref internalize to be available. */
        void set_url(raw_str_view str) const;
        void set_url(const std::string& str) const;
        void set_channel(raw_str_view str) const;
        void set_channel(const std::string& str) const;
        void set_subdir(raw_str_view str) const;
        void set_subdir(const std::string& str) const;

        void clear(bool reuse_ids) const;

        /**
         * Read repository information from file.
         *
         * @param solv_file A standard path with system encoding.
         */
        void read(std::filesystem::path solv_file) const;

        auto add_solvable() const -> std::pair<SolvableId, ObjSolvableView>;
        auto get_solvable(SolvableId id) const -> std::optional<ObjSolvableView>;
        auto remove_solvable(SolvableId id, bool reuse_id) const -> bool;

        template <typename UnaryFunc>
        void for_each_solvable(UnaryFunc func) const;

        /**
         * Internalize added data.
         *
         * Data must be internalized before it is available for lookup.
         * This concerns data added on solvable too.
         * This is a costly operation.
         */
        void internalize();
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

    template <typename UnaryFunc>
    void ObjRepoViewConst::for_each_solvable(UnaryFunc func) const
    {
        const ::Repo* const repo = raw();
        SolvableId id = 0;
        const ::Solvable* s = nullptr;
        FOR_REPO_SOLVABLES(repo, id, s)
        {
            func(ObjSolvableViewConst(s));
        }
    }

    template <typename UnaryFunc>
    void ObjRepoView::for_each_solvable(UnaryFunc func) const
    {
        const ::Repo* const repo = raw();
        SolvableId id = 0;
        ::Solvable* s = nullptr;
        FOR_REPO_SOLVABLES(repo, id, s)
        {
            func(ObjSolvableView(s));
        }
    }

}
#endif
