// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_REPO_HPP
#define MAMBA_SOLV_REPO_HPP

#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include <solv/repo.h>
#include <tl/expected.hpp>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/solvable.hpp"

namespace mamba::fs
{
    class u8path;
}

namespace mamba::solv
{

    class ObjRepoViewConst
    {
    public:

        static auto of_solvable(ObjSolvableViewConst s) -> ObjRepoViewConst;

        explicit ObjRepoViewConst(const ::Repo& repo) noexcept;
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
         * The etag header associated with the url.
         *
         * @see ObjRepoView::set_etag
         */
        auto etag() const -> std::string_view;

        /**
         * The mod header associated with the url.
         *
         * @see ObjRepoView::set_mod
         */
        auto mod() const -> std::string_view;

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

        /**
         * Whether pip was added to Python dependencies and vis versa.
         *
         * @see ObjRepoView::set_pip_added
         */
        auto pip_added() const -> bool;

        /**
         * The version used for writing solv files.
         *
         * @see ObjRepoView::set_too_version.
         */
        auto tool_version() const -> std::string_view;

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
         * @param solv_file A file pointer opened in binary mode.
         * @warning This is a binary file that is not portable and may not even remain valid among
         *          different libsolv build, let alone versions.
         */
        [[nodiscard]] auto write(std::FILE* solv_file) const -> tl::expected<void, std::string>;

    private:

        const ::Repo* m_repo = nullptr;
    };

    class ObjRepoView : public ObjRepoViewConst
    {
    public:

        using raw_str_view = const char*;

        explicit ObjRepoView(::Repo& repo) noexcept;

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
         * Set the etag associated with the url header.
         *
         * This has no effect for libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_etag(raw_str_view str) const;
        void set_etag(const std::string& str) const;

        /**
         * Set the mod associated with the url header.
         *
         * This has no effect for libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_mod(raw_str_view str) const;
        void set_mod(const std::string& str) const;

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
         * Set whether pip was added as a Python dependency and vice versa.
         *
         * This has no effect for libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_pip_added(bool b) const;

        /**
         * Set the version used for writing solv files.
         *
         * This has no effect for libsolv and is purely for data storing.
         * It is up to the user to make comparsions with this attribute.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_tool_version(raw_str_view str) const;
        void set_tool_version(const std::string& str) const;

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
         * @param solv_file A file pointer opened in binary mode.
         * @see ObjRepoViewConst::write
         */
        [[nodiscard]] auto read(std::FILE* solv_file) const -> tl::expected<void, std::string>;

        /**
         * Read repository information from a conda repodata.json.
         *
         * This function is part of libsolv and does not read all attributes of the repodata.
         * It is meant to be replaced. Parsing should be done by the user and solvables
         * added through the API.
         * @param repodata_file A file pointer opened in binary mode.
         */
        auto legacy_read_conda_repodata(std::FILE* repodata_file, int flags = 0) const
            -> tl::expected<void, std::string>;

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
            if constexpr (std::is_same_v<decltype(func(id)), LoopControl>)
            {
                if (func(id) == LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(id);
            }
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
            auto solvable = ObjSolvableViewConst{ *s };
            if constexpr (std::is_same_v<decltype(func(solvable)), LoopControl>)
            {
                if (func(solvable) == LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(solvable);
            }
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
            auto solvable = ObjSolvableView{ *s };
            if constexpr (std::is_same_v<decltype(func(solvable)), LoopControl>)
            {
                if (func(solvable) == LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(solvable);
            }
        }
    }

}
#endif
