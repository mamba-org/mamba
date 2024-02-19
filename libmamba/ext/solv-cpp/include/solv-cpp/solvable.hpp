// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_SOLVABLE_HPP
#define MAMBA_SOLV_SOLVABLE_HPP

#include <string>
#include <string_view>

#include <solv/pooltypes.h>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/queue.hpp"

extern "C"
{
    using Solvable = struct s_Solvable;
}

namespace solv
{
    /**
     * We use solvable for all sort of things, including virtual packages and pins.
     */
    enum class SolvableType : unsigned long long
    {
        Package,
        Virtualpackage,
        Pin,
    };

    class ObjSolvableViewConst
    {
    public:

        explicit ObjSolvableViewConst(const ::Solvable& solvable) noexcept;
        ~ObjSolvableViewConst() noexcept;

        auto raw() const -> const ::Solvable*;

        auto id() const -> SolvableId;

        /** The package name of the solvable. */
        auto name() const -> std::string_view;

        /** The package version of the solvable. */
        auto version() const -> std::string_view;

        auto build_number() const -> std::size_t;
        auto build_string() const -> std::string_view;
        auto file_name() const -> std::string_view;
        auto license() const -> std::string_view;
        auto md5() const -> std::string_view;
        auto noarch() const -> std::string_view;
        auto sha256() const -> std::string_view;
        auto signatures() const -> std::string_view;
        auto size() const -> std::size_t;
        auto timestamp() const -> std::size_t;

        /**
         * The url of the solvable.
         *
         * @see ObjSolvableView::set_url
         **/
        auto url() const -> std::string_view;

        /**
         * The channel of the solvable.
         *
         * @see ObjSolvableView::set_channel
         **/
        auto channel() const -> std::string_view;

        /**
         * The sub-directory of the solvable.
         *
         * @see ObjSolvableView::set_subdir
         **/
        auto subdir() const -> std::string_view;

        /**
         * Queue of ``DependencyId``.
         *
         * When the array is split in two using a maker, @p marker can be used to get
         * only a part of the the dependency array.
         * Use ``-1`` to get the first part, ``1`` to get the second part, and ``0`` to get
         * eveything including the marker.
         */
        auto dependencies(DependencyMarker marker = -1) const -> ObjQueue;

        /** Queue of ``DependencyId``. */
        auto provides() const -> ObjQueue;

        /** Queue of ``DependencyId``. */
        auto constraints() const -> ObjQueue;

        /** Queue of ``StringId``. */
        auto track_features() const -> ObjQueue;

        /** Whether the solvable is in the installed repo. */
        auto installed() const -> bool;

        /** The type for which the solvable is used. */
        auto type() const -> SolvableType;

    private:

        const ::Solvable* m_solvable = nullptr;
    };

    class ObjSolvableView : public ObjSolvableViewConst
    {
    public:

        using raw_str_view = const char*;

        explicit ObjSolvableView(::Solvable& repo) noexcept;

        auto raw() const -> ::Solvable*;

        void set_name(StringId id) const;
        void set_name(std::string_view str) const;
        void set_version(StringId id) const;
        void set_version(std::string_view str) const;

        /**
         * Set the build number of the solvable.
         *
         * @warning The pool must be of type conda for this to have an impact in during solving
         *          @ref ``ObjPool::set_disttype``.
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_build_number(std::size_t n) const;

        /**
         * Set the build string of the solvable.
         *
         * This is not used by libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_build_string(std::string_view bld) const;

        /**
         * Set the file name of the solvable.
         *
         * This is not used by libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_file_name(raw_str_view fn) const;
        void set_file_name(const std::string& fn) const;

        /**
         * Set the license of the solvable.
         *
         * This is not used by libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_license(raw_str_view str) const;
        void set_license(const std::string& str) const;

        /**
         * Set the md5 hash of the solvable file.
         *
         * This is not used by libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_md5(raw_str_view str) const;
        void set_md5(const std::string& str) const;

        /**
         * Set the noarch type of the solvable.
         *
         * This is not used by libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_noarch(raw_str_view str) const;
        void set_noarch(const std::string& str) const;

        /**
         * Set the sha256 hash of the solvable file.
         *
         * This is not used by libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_sha256(raw_str_view str) const;
        void set_sha256(const std::string& str) const;

        /**
         * Set the signatures of the solvable file.
         */
        void set_signatures(raw_str_view str) const;
        void set_signatures(const std::string& str) const;

        /**
         * Set the size of the solvable size.
         *
         * This is not used by libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_size(std::size_t n) const;

        /**
         * Set the timestamp of the solvable.
         *
         * This is not used by libsolv and is purely for data storing.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_timestamp(std::size_t n) const;

        /**
         * Set the url of the solvable.
         *
         * This has no effect for libsolv and is purely for data storing.
         * This may not be the same as @ref ObjRepoViewConst::url, for instance the install
         * repository may have no url but its packages do.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_url(raw_str_view str) const;
        void set_url(const std::string& str) const;

        /**
         * Set the channel of the solvable.
         *
         * This has no effect for libsolv and is purely for data storing.
         * This may not be the same as @ref ObjRepoViewConst::channel, for instance the install
         * repository may have no channel but its packages do.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_channel(raw_str_view str) const;
        void set_channel(const std::string& str) const;

        /**
         * Set the sub-directory of the solvable.
         *
         * This has no effect for libsolv and is purely for data storing.
         * This may not be the same as @ref ObjRepoViewConst::channel, for instance the install
         * repository may have no subdir but its packages do.
         *
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         */
        void set_subdir(raw_str_view str) const;
        void set_subdir(const std::string& str) const;

        /** Set the dependencies of the solvable. */
        void set_dependencies(const ObjQueue& q, DependencyMarker marker = 0) const;

        /**
         * Add a additional dependency to the solvable.
         *
         * Dependency array can be split in two using the given @p marker.
         *
         * @see dependencies
         */
        void add_dependency(DependencyId dep, DependencyMarker marker = 0) const;

        /** Add multiple additional dependencies to the solvable. */
        template <typename Iter>
        void add_dependencies(Iter first, Iter last) const;
        template <typename Range>
        void add_dependencies(const Range& deps) const;

        /**
         * Set the provides list of a solvable.
         *
         * In libsolv, a dependency is not match against a solvable name but against its
         * ``provides``.
         * A package can provide multiple names, for instance an ``openblas 1.0.0`` package
         * could provide ```openblas==1.0.0`` and ``blas<=1.5``.
         * This is not possible in conda, hence we always use the
         * @ref ObjSolvableView::add_self_provide function.
         */
        void set_provides(const ObjQueue& q) const;

        /**
         * Add an additional provide to the sovable.
         *
         * @see ObjSolvableView::set_provides.
         */
        void add_provide(DependencyId dep) const;

        /**
         * Add an additional provide to the sovable stating the solvable provides itself.
         *
         * Namely the solvable ``s`` provides ``s.name() == s.version()``.
         *
         * @see ObjSolvableView::set_provides.
         */
        void add_self_provide() const;

        /** Add multiple provides to the solvable. */
        template <typename Iter>
        void add_provides(Iter first, Iter last) const;
        template <typename Range>
        void add_provides(const Range& deps) const;

        /**
         * Set all constraints.
         *
         * A constraint is like a dependency that is not part of the solving outcome.
         * In other words, if a solvable has a constraint, it is activated ony if another solvable
         * in the solving requires that package as a dependencyl
         *
         * @warning The pool must be of type conda for this to have an impact in during solving
         *          @ref ``ObjPool::set_disttype``.
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         **/
        void set_constraints(const ObjQueue& q) const;

        /**
         * Add a constraint.
         *
         * This function can be used to add constraints one by one.

         * @warning The pool must be of type conda for this to have an impact in during solving
         *          @ref ``ObjPool::set_disttype``.
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         *       If some constraints were already internalized, the effect of this function is to
         *       start a new set of constraints that will replace the old ones rather than add
         *       to it.
         */
        void add_constraint(DependencyId dep) const;
        template <typename Iter>

        /**
         * Add multiple constraints to the solvable.
         *
         * @warning The pool must be of type conda for this to have an impact in during solving
         *          @ref ``ObjPool::set_disttype``.
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         * @see ObjSolvableView::add_constraint.
         */
        void add_constraints(Iter first, Iter last) const;
        template <typename Range>
        void add_constraints(const Range& deps) const;

        /**
         * Set all track features.
         *
         * The number of track features is used to de-prioritize solvable during solving.
         *
         * @warning The pool must be of type conda for this to have an impact in during solving
         *          @ref ``ObjPool::set_disttype``.
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         * @param q A queue of of pool @ref StringId
         **/
        void set_track_features(const ObjQueue& q) const;

        /**
         * Add a tracked feature.
         *
         * This function can be used to add tracked feature one by one.
         *
         * @warning The pool must be of type conda for this to have an impact in during solving
         *          @ref ``ObjPool::set_disttype``.
         * @note A call to @ref ObjRepoView::internalize is required for this attribute to
         *       be available for lookup.
         *       If some constraints were already internalized, the effect of this function is to
         *       start a new set of constraints that will replace the old ones rather than add
         *       to it.
         * @see ObjSolvableView::add_track_feature
         * @param feat A pool @ref StringId of the feature string.
         * @return The same id as @p feat.
         */
        auto add_track_feature(StringId feat) const -> StringId;

        /**
         * Add a tracked feature from a string.
         *
         * @see add_track_feature
         * @param feat The feature to add is also added to the pool.
         * @return The pool @ref StringId associated with @p feat.
         */
        auto add_track_feature(std::string_view feat) const -> StringId;

        /**
         * Add multiple track features to the solvable.
         *
         * The range can contain strings or StringId.
         *
         * @see add_track_feature
         */
        template <typename Iter>
        void add_track_features(Iter first, Iter last) const;
        template <typename Range>
        void add_track_features(const Range& features) const;

        /**
         * Mark mark the package as being of a specific type.
         *
         * @see ObjSolvableViewConst::type
         */
        void set_type(SolvableType val) const;
    };

    /***************************************
     *  Implementation of ObjSolvableView  *
     ***************************************/

    template <typename Iter>
    void ObjSolvableView::add_dependencies(Iter first, Iter last) const
    {
        for (; first != last; ++first)
        {
            add_dependency(*first);
        }
    }

    template <typename Range>
    void ObjSolvableView::add_dependencies(const Range& deps) const
    {
        return add_dependencies(deps.cbegin(), deps.cend());
    }

    template <typename Iter>
    void ObjSolvableView::add_provides(Iter first, Iter last) const
    {
        for (; first != last; ++first)
        {
            add_provide(*first);
        }
    }

    template <typename Range>
    void ObjSolvableView::add_provides(const Range& deps) const
    {
        return add_provides(deps.cbegin(), deps.cend());
    }

    template <typename Iter>
    void ObjSolvableView::add_constraints(Iter first, Iter last) const
    {
        for (; first != last; ++first)
        {
            add_constraint(*first);
        }
    }

    template <typename Range>
    void ObjSolvableView::add_constraints(const Range& deps) const
    {
        return add_constraints(deps.cbegin(), deps.cend());
    }

    template <typename Iter>
    void ObjSolvableView::add_track_features(Iter first, Iter last) const
    {
        for (; first != last; ++first)
        {
            add_track_feature(*first);
        }
    }

    template <typename Range>
    void ObjSolvableView::add_track_features(const Range& feats) const
    {
        return add_track_features(feats.cbegin(), feats.cend());
    }
}
#endif
