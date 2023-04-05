// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_SOLVABLE_HPP
#define MAMBA_SOLV_SOLVABLE_HPP

#include <string>
#include <string_view>
#include <utility>

#include <solv/pooltypes.h>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/queue.hpp"

typedef struct s_Solvable Solvable;

namespace mamba::solv
{
    class ObjSolvableViewConst
    {
    public:

        explicit ObjSolvableViewConst(const ::Solvable* solvable) noexcept;

        auto raw() const -> const ::Solvable*;

        auto name() const -> std::string_view;
        auto version() const -> std::string_view;

        auto build_number() const -> std::size_t;
        auto build_string() const -> std::string_view;
        auto file_name() const -> std::string_view;
        auto license() const -> std::string_view;
        auto md5() const -> std::string_view;
        auto noarch() const -> std::string_view;
        auto sha256() const -> std::string_view;
        auto size() const -> std::size_t;
        auto timestamp() const -> std::size_t;

        /** Queue of ``DependencyId``. */
        auto dependencies() const -> ObjQueue;
        /** Queue of ``DependencyId``. */
        auto provides() const -> ObjQueue;
        /** Queue of ``DependencyId``. */
        auto constraints() const -> ObjQueue;
        /** Queue of ``StringId``. */
        auto track_features() const -> ObjQueue;

    private:

        const ::Solvable* m_solvable = nullptr;
    };

    class ObjSolvableView : public ObjSolvableViewConst
    {
    public:

        using raw_str_view = const char*;

        explicit ObjSolvableView(::Solvable* repo) noexcept;

        auto raw() const -> ::Solvable*;

        void set_name(std::string_view str) const;
        void set_version(std::string_view str) const;

        /** The following attributes need a call to @ref ObjRepoView::internalize. */
        void set_build_number(std::size_t n) const;
        void set_build_string(std::string_view bld) const;
        void set_file_name(raw_str_view fn) const;
        void set_file_name(const std::string& fn) const;
        void set_license(raw_str_view str) const;
        void set_license(const std::string& str) const;
        void set_md5(raw_str_view str) const;
        void set_md5(const std::string& str) const;
        void set_noarch(raw_str_view str) const;
        void set_noarch(const std::string& str) const;
        void set_sha256(raw_str_view str) const;
        void set_sha256(const std::string& str) const;
        void set_size(std::size_t n) const;
        void set_timestamp(std::size_t n) const;

        void set_dependencies(const ObjQueue& q) const;
        void add_dependency(DependencyId dep) const;
        template <typename Iter>
        void add_dependencies(Iter first, Iter last);
        template <typename Range>
        void add_dependencies(const Range& deps);

        void set_provides(const ObjQueue& q) const;
        void add_provide(DependencyId dep) const;
        void add_self_provide() const;
        template <typename Iter>
        void add_provides(Iter first, Iter last);
        template <typename Range>
        void add_provides(const Range& deps);

        /**
         * Set all constraints.
         *
         * Setting this attribute requires a call to @ref ObjRepoView::internalize before it
         * can be used.
         **/
        void set_constraints(const ObjQueue& q) const;
        /**
         * Add a constraint.
         *
         * This function can be used to add constraints one by one.
         * After all constraints have been added (or at a later time), a call to
         * @ref ObjRepoView::internalize is required before the constraints can be used.
         * If some constraints were already internalized, the effect of this function is to start
         * a new set of constraints that will replace the old one rather than add to it.
         */
        void add_constraint(DependencyId dep) const;
        template <typename Iter>
        void add_constraints(Iter first, Iter last);
        template <typename Range>
        void add_constraints(const Range& deps);

        /**
         * Set all track features.
         *
         * Setting this attribute requires a call to @ref ObjRepoView::internalize before it
         * can be used.
         *
         * @param q A queue of of pool @ref StringId
         **/
        void set_track_features(const ObjQueue& q) const;
        /**
         * Add a tracked feature.
         *
         * This function can be used to add tracked feature one by one.
         * After all tracked features have been added (or at a later time), a call to
         * @ref ObjRepoView::internalize is required before the tracked features can be used.
         * If some tracked feature were already internalized, the effect of this function is
         * to start a new set of constraints that will replace the old one rather than add to it.
         *
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
        template <typename Iter>
        void add_track_features(Iter first, Iter last);
        template <typename Range>
        void add_track_features(const Range& features);
    };

    /***************************************
     *  Implementation of ObjSolvableView  *
     ***************************************/

    template <typename Iter>
    void ObjSolvableView::add_dependencies(Iter first, Iter last)
    {
        for (; first != last; ++first)
        {
            add_dependency(*first);
        }
    }

    template <typename Range>
    void ObjSolvableView::add_dependencies(const Range& deps)
    {
        return add_dependencies(deps.cbegin(), deps.cend());
    }

    template <typename Iter>
    void ObjSolvableView::add_provides(Iter first, Iter last)
    {
        for (; first != last; ++first)
        {
            add_provide(*first);
        }
    }

    template <typename Range>
    void ObjSolvableView::add_provides(const Range& deps)
    {
        return add_provides(deps.cbegin(), deps.cend());
    }

    template <typename Iter>
    void ObjSolvableView::add_constraints(Iter first, Iter last)
    {
        for (; first != last; ++first)
        {
            add_constraint(*first);
        }
    }

    template <typename Range>
    void ObjSolvableView::add_constraints(const Range& deps)
    {
        return add_constraints(deps.cbegin(), deps.cend());
    }

    template <typename Iter>
    void ObjSolvableView::add_track_features(Iter first, Iter last)
    {
        for (; first != last; ++first)
        {
            add_track_feature(*first);
        }
    }

    template <typename Range>
    void ObjSolvableView::add_track_features(const Range& feats)
    {
        return add_track_features(feats.cbegin(), feats.cend());
    }
}
#endif
