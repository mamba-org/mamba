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

using Repo = struct s_Repo;

namespace mamba::solv
{

    class ObjRepoViewConst
    {
    public:

        explicit ObjRepoViewConst(const ::Repo* repo) noexcept;
        ~ObjRepoViewConst() noexcept;

        auto raw() const -> const ::Repo*;

        auto id() const -> RepoId;

        /** The name of the repository. */
        auto name() const -> std::string_view;

        /**
         * The url of the repository.
         *
         * @see ObjRepoView::set_url
         **/
        auto url() const -> std::string_view;

        /**
         * The channel of the repository.
         *
         * @see ObjRepoView::set_url
         **/
        auto channel() const -> std::string_view;

        /**
         * The sub-directory of the repository.
         *
         * @see ObjRepoView::set_url
         **/
        auto subdir() const -> std::string_view;

        /** The number of solvables in this repository. */
        auto solvable_count() const -> std::size_t;

        /** Check if a solvable exisists and is in this repository. */
        auto has_solvable(SolvableId id) const -> bool;

        /** Get the current solvable, if it exists and is in this repository. */
        auto get_solvable(SolvableId id) const -> std::optional<ObjSolvableViewConst>;

        /** Execute function of all solvable ids in this repository. */
        template <typename UnaryFunc>
        void for_each_solvable_id(UnaryFunc&& func) const;

        /** Execute function of all solvables in this repository. */
        template <typename UnaryFunc>
        void for_each_solvable(UnaryFunc&& func) const;

        /**
         * Write repository information to file.
         *
         * @param solv_file A standard path with system encoding.
         * @warning This is a binary file that is not portable and may not even remain valid among
         *          different libsolv build, let alone versions.
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

        /**
         * Set the url of the repository.
         *
         * This has no effect for libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_url(raw_str_view str) const;
        void set_url(const std::string& str) const;

        /**
         * Set the channel of the repository.
         *
         * This has no effect for libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_channel(raw_str_view str) const;
        void set_channel(const std::string& str) const;

        /**
         * Set the sub-directory of the repository.
         *
         * This has no effect for libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_subdir(raw_str_view str) const;
        void set_subdir(const std::string& str) const;


        /**
         * Clear all solvables from the repository.
         *
         * If @p reuse_ids is true, the solvable ids used in the pool can be reused for future
         * solvables (incuding in other repositories).
         */
        void clear(bool reuse_ids) const;

        /**
         * Read repository information from file.
         *
         * @param solv_file A standard path with system encoding.
         * @see ObjRepoViewConst::write
         */
        void read(std::filesystem::path solv_file) const;

        /** Add an empty solvable to the repository. */
        auto add_solvable() const -> std::pair<SolvableId, ObjSolvableView>;

        /** Get the current solvable, if it exists and is in this repository. */
        auto get_solvable(SolvableId id) const -> std::optional<ObjSolvableView>;

        /**
         * Remove a solvable from the repository.
         *
         * If @p reuse_id is true, the solvable id used in the pool can be reused for future
         * solvables (incuding in other repositories).
         */
        auto remove_solvable(SolvableId id, bool reuse_id) const -> bool;

        /** Execute function of all solvables in this repository. */
        template <typename UnaryFunc>
        void for_each_solvable(UnaryFunc&& func) const;

        /**
         * Internalize added data.
         *
         * Data must be internalized before it is available for lookup.
         * This concerns data added on solvable too.
         * @warning This is a costly operation, and should ideally be called once after
         *          all attributes are set on the repository and its solvables.
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
    void ObjRepoViewConst::for_each_solvable_id(UnaryFunc&& func) const
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
    void ObjRepoViewConst::for_each_solvable(UnaryFunc&& func) const
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
    void ObjRepoView::for_each_solvable(UnaryFunc&& func) const
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
