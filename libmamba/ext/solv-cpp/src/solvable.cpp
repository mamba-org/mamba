// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <limits>

#include <solv/knownid.h>
#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/solvable.h>

#include "solv-cpp/solvable.hpp"

namespace solv
{
    /********************************************
     *  Implementation of ConstObjSolvableView  *
     ********************************************/

    ObjSolvableViewConst::ObjSolvableViewConst(const ::Solvable& solvable) noexcept
        : m_solvable{ &solvable }
    {
    }

    ObjSolvableViewConst::~ObjSolvableViewConst() noexcept
    {
        m_solvable = nullptr;
    }

    auto ObjSolvableViewConst::raw() const -> const ::Solvable*
    {
        return m_solvable;
    }

    auto ObjSolvableViewConst::id() const -> SolvableId
    {
        return ::pool_solvable2id(raw()->repo->pool, const_cast<::Solvable*>(raw()));
    }

    /***************************************
     *  Implementation of ObjSolvableView  *
     ***************************************/

    ObjSolvableView::ObjSolvableView(::Solvable& solvable) noexcept
        : ObjSolvableViewConst{ solvable }
    {
    }

    auto ObjSolvableView::raw() const -> ::Solvable*
    {
        // Safe because we were passed a ``Solvable*`` at construction time.
        return const_cast<::Solvable*>(ObjSolvableViewConst::raw());
    }

    /******************************************
     *  Implementation of getter and setters  *
     ******************************************/

    namespace
    {
        auto ptr_to_strview(const char* ptr) -> std::string_view
        {
            static constexpr std::string_view null = "<NULL>";
            if ((ptr == nullptr) || (ptr == null))
            {
                return {};
            }
            return { ptr };
        }

        /**
         * Add a string to the solvable pool.
         *
         * When we know that the solvable attribute needs to be a pool id, we can use this
         * function to leverage the ``pool_strn2id`` function with a `std::string_view``.
         */
        auto solvable_add_pool_str(::Pool* pool, std::string_view value)
        {
            assert(value.size() <= std::numeric_limits<unsigned int>::max());
            const ::Id id = ::pool_strn2id(
                pool,
                value.data(),
                static_cast<unsigned int>(value.size()),
                /* .create= */ 1
            );
            assert(id != 0);
            return id;
        }
    }

    auto ObjSolvableViewConst::name() const -> std::string_view
    {
        return ptr_to_strview(::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_NAME));
    }

    void ObjSolvableView::set_name(StringId id) const
    {
        ::solvable_set_id(raw(), SOLVABLE_NAME, id);
    }

    void ObjSolvableView::set_name(std::string_view str) const
    {
        return set_name(solvable_add_pool_str(raw()->repo->pool, str));
    }

    auto ObjSolvableViewConst::version() const -> std::string_view
    {
        return ptr_to_strview(::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_EVR));
    }

    void ObjSolvableView::set_version(StringId id) const
    {
        ::solvable_set_id(raw(), SOLVABLE_EVR, id);
    }

    void ObjSolvableView::set_version(std::string_view str) const
    {
        return set_version(solvable_add_pool_str(raw()->repo->pool, str));
    }

    namespace
    {
        template <typename Int>
        auto to_int_or(std::string_view str, Int val) -> Int
        {
            std::from_chars(str.data(), str.data() + str.size(), val);
            return val;
        }
    }

    auto ObjSolvableViewConst::build_number() const -> std::size_t
    {
        return to_int_or<std::size_t>(
            ptr_to_strview(::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_BUILDVERSION)
            ),
            0
        );
    }

    void ObjSolvableView::set_build_number(std::size_t n) const
    {
        // SOLVABLE_BUILDVERSION must be set as a string for libsolv to understand it.

        // No easy way to compute the number of digits needed for a 64 bit unsigned integer
        // at compile time
        static_assert(
            std::numeric_limits<decltype(n)>::max() <= std::numeric_limits<std::uint64_t>::max()
        );
        static constexpr auto n_digits = 20;

        auto str = std::array<char, n_digits + 1>{};  // +1 for null termination
        str.fill('\0');
        [[maybe_unused]] const auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), n);
        assert(ec == std::errc());
        assert(ptr != nullptr);
        ::solvable_set_str(raw(), SOLVABLE_BUILDVERSION, str.data());
    }

    auto ObjSolvableViewConst::build_string() const -> std::string_view
    {
        return ptr_to_strview(
            ::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_BUILDFLAVOR)
        );
    }

    void ObjSolvableView::set_build_string(std::string_view bld) const
    {
        ::solvable_set_id(raw(), SOLVABLE_BUILDFLAVOR, solvable_add_pool_str(raw()->repo->pool, bld));
    }

    auto ObjSolvableViewConst::file_name() const -> std::string_view
    {
        return ptr_to_strview(::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_MEDIAFILE)
        );
    }

    void ObjSolvableView::set_file_name(raw_str_view fn) const
    {
        // TODO would like to use ``repodata_set_strn`` or similar but it is not visible
        ::solvable_set_str(raw(), SOLVABLE_MEDIAFILE, fn);
    }

    void ObjSolvableView::set_file_name(const std::string& fn) const
    {
        return set_file_name(fn.c_str());
    }

    auto ObjSolvableViewConst::license() const -> std::string_view
    {
        return ptr_to_strview(::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_LICENSE));
    }

    void ObjSolvableView::set_license(raw_str_view str) const
    {
        ::solvable_set_str(raw(), SOLVABLE_LICENSE, str);
    }

    void ObjSolvableView::set_license(const std::string& str) const
    {
        return set_license(str.c_str());
    }

    auto ObjSolvableViewConst::md5() const -> std::string_view
    {
        ::Id type = 0;
        const char* hash = ::solvable_lookup_checksum(
            const_cast<::Solvable*>(raw()),
            SOLVABLE_PKGID,
            &type
        );
        assert((type == REPOKEY_TYPE_MD5) || (hash == nullptr));
        return ptr_to_strview(hash);
    }

    void ObjSolvableView::set_md5(raw_str_view str) const
    {
        ::Repo* repo = raw()->repo;
        ::repodata_set_checksum(
            ::repo_last_repodata(repo),
            ::pool_solvable2id(repo->pool, raw()),
            SOLVABLE_PKGID,
            REPOKEY_TYPE_MD5,
            str
        );
    }

    void ObjSolvableView::set_md5(const std::string& str) const
    {
        return set_md5(str.c_str());
    }

    auto ObjSolvableViewConst::noarch() const -> std::string_view
    {
        return ptr_to_strview(
            ::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_SOURCEARCH)
        );
    }

    void ObjSolvableView::set_noarch(raw_str_view str) const
    {
        ::solvable_set_str(raw(), SOLVABLE_SOURCEARCH, str);
    }

    void ObjSolvableView::set_noarch(const std::string& str) const
    {
        return set_noarch(str.c_str());
    }

    auto ObjSolvableViewConst::sha256() const -> std::string_view
    {
        ::Id type = 0;
        const char* hash = ::solvable_lookup_checksum(
            const_cast<::Solvable*>(raw()),
            SOLVABLE_CHECKSUM,
            &type
        );
        assert((type == REPOKEY_TYPE_SHA256) || (hash == nullptr));
        return ptr_to_strview(hash);
    }

    void ObjSolvableView::set_sha256(raw_str_view str) const
    {
        ::Repo* repo = raw()->repo;
        ::repodata_set_checksum(
            ::repo_last_repodata(repo),
            ::pool_solvable2id(repo->pool, raw()),
            SOLVABLE_CHECKSUM,
            REPOKEY_TYPE_SHA256,
            str
        );
    }

    void ObjSolvableView::set_sha256(const std::string& str) const
    {
        return set_sha256(str.c_str());
    }

    auto ObjSolvableViewConst::signatures() const -> std::string_view
    {
        // NOTE This returns the package signatures json object alongside other package info
        // in the following format:
        // {"info":{},"signatures":{"public_key":{"signature":"metadata_signature"}}}
        return ptr_to_strview(
            ::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_SIGNATUREDATA)
        );
    }

    void ObjSolvableView::set_signatures(raw_str_view str) const
    {
        ::solvable_set_str(raw(), SOLVABLE_SIGNATUREDATA, str);
    }

    void ObjSolvableView::set_signatures(const std::string& str) const
    {
        return set_signatures(str.c_str());
    }

    auto ObjSolvableViewConst::size() const -> std::size_t
    {
        return ::solvable_lookup_num(const_cast<::Solvable*>(raw()), SOLVABLE_DOWNLOADSIZE, 0);
    }

    void ObjSolvableView::set_size(std::size_t n) const
    {
        ::solvable_set_num(raw(), SOLVABLE_DOWNLOADSIZE, n);
    }

    auto ObjSolvableViewConst::timestamp() const -> std::size_t
    {
        return ::solvable_lookup_num(const_cast<::Solvable*>(raw()), SOLVABLE_BUILDTIME, 0);
    }

    void ObjSolvableView::set_timestamp(std::size_t n) const
    {
        ::solvable_set_num(raw(), SOLVABLE_BUILDTIME, n);
    }

    auto ObjSolvableViewConst::url() const -> std::string_view
    {
        return ptr_to_strview(::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_URL));
    }

    void ObjSolvableView::set_url(raw_str_view str) const
    {
        ::solvable_set_str(raw(), SOLVABLE_URL, str);
    }

    void ObjSolvableView::set_url(const std::string& str) const
    {
        return set_url(str.c_str());
    }

    auto ObjSolvableViewConst::channel() const -> std::string_view
    {
        return ptr_to_strview(::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_PACKAGER)
        );
    }

    void ObjSolvableView::set_channel(raw_str_view str) const
    {
        ::solvable_set_str(raw(), SOLVABLE_PACKAGER, str);
    }

    void ObjSolvableView::set_channel(const std::string& str) const
    {
        return set_channel(str.c_str());
    }

    auto ObjSolvableViewConst::platform() const -> std::string_view
    {
        return ptr_to_strview(::solvable_lookup_str(const_cast<::Solvable*>(raw()), SOLVABLE_MEDIADIR)
        );
    }

    void ObjSolvableView::set_platform(raw_str_view str) const
    {
        ::solvable_set_str(raw(), SOLVABLE_MEDIADIR, str);
    }

    void ObjSolvableView::set_platform(const std::string& str) const
    {
        return set_platform(str.c_str());
    }

    auto ObjSolvableViewConst::dependencies(DependencyMarker marker) const -> ObjQueue
    {
        auto q = ObjQueue{};
        ::solvable_lookup_deparray(const_cast<::Solvable*>(raw()), SOLVABLE_REQUIRES, q.raw(), marker);
        return q;
    }

    void ObjSolvableView::set_dependencies(const ObjQueue& q, DependencyMarker marker) const
    {
        ::solvable_set_deparray(raw(), SOLVABLE_REQUIRES, const_cast<::Queue*>(q.raw()), marker);
    }

    void ObjSolvableView::add_dependency(DependencyId dep, DependencyMarker marker) const
    {
        raw()->requires = ::repo_addid_dep(raw()->repo, raw()->requires, dep, marker);
    }

    auto ObjSolvableViewConst::provides() const -> ObjQueue
    {
        auto q = ObjQueue{};
        ::solvable_lookup_deparray(
            const_cast<::Solvable*>(raw()),
            SOLVABLE_PROVIDES,
            q.raw(),
            /* marker= */ -1
        );
        return q;
    }

    void ObjSolvableView::set_provides(const ObjQueue& q) const
    {
        ::solvable_set_deparray(
            raw(),
            SOLVABLE_PROVIDES,
            const_cast<::Queue*>(q.raw()),
            /* marker= */ 0
        );
    }

    void ObjSolvableView::add_provide(DependencyId dep) const
    {
        raw()->provides = ::repo_addid_dep(raw()->repo, raw()->provides, dep, /* marker= */ 0);
    }

    void ObjSolvableView::add_self_provide() const
    {
        return add_provide(
            ::pool_rel2id(raw()->repo->pool, raw()->name, raw()->evr, REL_EQ, /* create= */ 1)
        );
    }

    auto ObjSolvableViewConst::constraints() const -> ObjQueue
    {
        auto q = ObjQueue{};
        ::solvable_lookup_deparray(const_cast<::Solvable*>(raw()), SOLVABLE_CONSTRAINS, q.raw(), -1);
        return q;
    }

    void ObjSolvableView::set_constraints(const ObjQueue& q) const
    {
        // Prevent weird behavior when q is empty
        if (q.empty())
        {
            ::solvable_unset(raw(), SOLVABLE_CONSTRAINS);
        }
        else
        {
            ::solvable_set_deparray(
                raw(),
                SOLVABLE_CONSTRAINS,
                const_cast<::Queue*>(q.raw()),
                /* marker= */ -1
            );
        }
    }

    void ObjSolvableView::add_constraint(DependencyId dep) const
    {
        ::solvable_add_idarray(raw(), SOLVABLE_CONSTRAINS, dep);
    }

    auto ObjSolvableViewConst::track_features() const -> ObjQueue
    {
        auto q = ObjQueue{};
        ::solvable_lookup_idarray(const_cast<::Solvable*>(raw()), SOLVABLE_TRACK_FEATURES, q.raw());
        return q;
    }

    void ObjSolvableView::set_track_features(const ObjQueue& q) const
    {
        // Prevent weird behavior when q is empty
        if (q.empty())
        {
            ::solvable_unset(raw(), SOLVABLE_TRACK_FEATURES);
        }
        else
        {
            ::solvable_set_idarray(raw(), SOLVABLE_TRACK_FEATURES, const_cast<::Queue*>(q.raw()));
        }
    }

    auto ObjSolvableView::add_track_feature(StringId feat) const -> StringId
    {
        ::solvable_add_idarray(raw(), SOLVABLE_TRACK_FEATURES, feat);
        return feat;
    }

    auto ObjSolvableView::add_track_feature(std::string_view feat) const -> StringId
    {
        return add_track_feature(solvable_add_pool_str(raw()->repo->pool, feat));
    }

    auto ObjSolvableViewConst::installed() const -> bool
    {
        const auto* const repo = raw()->repo;
        return (repo != nullptr) && (repo == repo->pool->installed);
    }

    auto ObjSolvableViewConst::type() const -> SolvableType
    {
        using Num = std::underlying_type_t<SolvableType>;
        // (Ab)using meaningless key
        return static_cast<SolvableType>(::solvable_lookup_num(
            const_cast<::Solvable*>(raw()),
            SOLVABLE_INSTALLSTATUS,
            static_cast<Num>(SolvableType::Package)
        ));
    }

    void ObjSolvableView::set_type(SolvableType val) const
    {
        using Num = std::underlying_type_t<SolvableType>;
        // (Ab)using meaningless key
        ::solvable_set_num(raw(), SOLVABLE_INSTALLSTATUS, static_cast<Num>(val));
    }
}
