// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <exception>
#include <sstream>

#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/repo_solv.h>
#include <solv/repo_write.h>

#include "solv-cpp/repo.hpp"

namespace mamba::solv
{
    /****************************************
     *  Implementation of ConstObjRepoView  *
     ****************************************/

    ObjRepoViewConst::ObjRepoViewConst(const ::Repo* repo) noexcept
        : m_repo{ repo }
    {
    }

    auto ObjRepoViewConst::raw() const -> const ::Repo*
    {
        return m_repo;
    }

    auto ObjRepoViewConst::id() const -> RepoId
    {
        // No function exists to get a repo id
        const ::Pool* pool = raw()->pool;
        const ::Repo* repo = nullptr;
        int repoid = 0;
        FOR_REPOS(repoid, repo)
        {
            if (repo == raw())
            {
                return repoid;
            }
        }
        assert(false);
        return 0;
    }

    auto ObjRepoViewConst::n_solvables() const -> std::size_t
    {
        assert(raw()->nsolvables >= 0);
        return static_cast<std::size_t>(raw()->nsolvables);
    }

    auto ObjRepoViewConst::contains_solvable(SolvableId id) const -> bool
    {
        const ::Repo* const repo = raw();
        SolvableId other_id = 0;
        const ::Solvable* s = nullptr;
        FOR_REPO_SOLVABLES(repo, other_id, s)
        {
            if (other_id == id)
            {
                return true;
            }
        }
        return false;
    }

    auto ObjRepoViewConst::get_solvable(SolvableId id) const -> ObjSolvableViewConst
    {
        return ObjSolvableViewConst{ ::pool_id2solvable(raw()->pool, id) };
    }

    void ObjRepoViewConst::write(std::filesystem::path solv_file) const
    {
        auto* fp = std::fopen(solv_file.string().c_str(), "wb");
        if (!fp)
        {
            throw std::system_error(errno, std::generic_category());
        }
        const auto write_res = ::repo_write(const_cast<::Repo*>(raw()), fp);
        const auto close_res = std::fclose(fp);
        if (write_res != 0)
        {
            // TODO(C++20) fmt::format
            auto ss = std::stringstream();
            ss << "Unable to write repo '" << name() << "' to file";
            throw std::runtime_error(ss.str());
        }
        if (close_res != 0)
        {
            // TODO(C++20) fmt::format
            auto ss = std::stringstream();
            ss << "Unable to write file '" << solv_file << '\'';
            throw std::runtime_error(ss.str());
        }
    }

    /***********************************
     *  Implementation of ObjRepoView  *
     ***********************************/

    ObjRepoView::ObjRepoView(::Repo* repo) noexcept
        : ObjRepoViewConst{ repo }
    {
    }

    auto ObjRepoView::raw() const -> ::Repo*
    {
        // Safe because we were passed a ``Repo*`` at construction time.
        return const_cast<::Repo*>(ObjRepoViewConst::raw());
    }

    void ObjRepoView::clear(bool reuse_ids) const
    {
        ::repo_empty(raw(), static_cast<int>(reuse_ids));
    }

    void ObjRepoView::read(std::filesystem::path solv_file) const
    {
        auto* fp = std::fopen(solv_file.string().c_str(), "rb");
        if (!fp)
        {
            throw std::system_error(errno, std::generic_category());
        }
        const auto read_res = ::repo_add_solv(raw(), fp, 0);
        const auto close_res = std::fclose(fp);
        if (read_res != 0)
        {
            // TODO(C++20) fmt::format
            auto ss = std::stringstream();
            ss << "Unable to read repo solv file '" << name() << '\'';
            throw std::runtime_error(ss.str());
        }
        if (close_res != 0)
        {
            // TODO(C++20) fmt::format
            auto ss = std::stringstream();
            ss << "Unable to read from file '" << solv_file << '\'';
            throw std::runtime_error(ss.str());
        }
    }

    auto ObjRepoView::add_solvable() const -> SolvableId
    {
        return ::repo_add_solvable(raw());
    }

    auto ObjRepoView::get_solvable(SolvableId id) const -> ObjSolvableView
    {
        return ObjSolvableView{ ::pool_id2solvable(raw()->pool, id) };
    }

    void ObjRepoView::remove_solvable(SolvableId id, bool reuse_id) const
    {
        ::repo_free_solvable(raw(), id, reuse_id);
    }

    void ObjRepoView::internalize()
    {
        ::repo_internalize(raw());
    }

    /****************************************
     *  Implementation of getter & setters  *
     ****************************************/

    auto ObjRepoViewConst::name() const -> std::string_view
    {
        return ::repo_name(raw());
    }

    namespace
    {
        auto ptr_to_strview(const char* ptr) -> std::string_view
        {
            static constexpr std::string_view null = "<NULL>";
            if ((ptr == nullptr) || (ptr == null))
            {
                return "";
            }
            return { ptr };
        }

        auto repo_lookup_str(const ::Repo* repo, ::Id key) -> std::string_view
        {
            // Can only read key/value on solvable, but the special SOLVID_META is used for a fake
            // solvable representing the repo.
            // The key used does not really matter so we can (ab)use any key that does not have
            // special meaning
            return ptr_to_strview(::repo_lookup_str(const_cast<::Repo*>(repo), SOLVID_META, key));
        }

        void repo_set_str(::Repo* repo, ::Id key, const char* str)
        {
            // Can only read key/value on solvable, but the special SOLVID_META is used for a fake
            // solvable representing the repo.
            // The key used does not really matter so we can (ab)use any key that does not have
            // special meaning
            ::repo_set_str(repo, SOLVID_META, key, str);
        }
    }

    auto ObjRepoViewConst::url() const -> std::string_view
    {
        return repo_lookup_str(raw(), SOLVABLE_URL);
    }

    void ObjRepoView::set_url(raw_str_view str) const
    {
        return repo_set_str(raw(), SOLVABLE_URL, str);
    }

    void ObjRepoView::set_url(const std::string& str) const
    {
        return set_url(str.c_str());
    }

    auto ObjRepoViewConst::channel() const -> std::string_view
    {
        return repo_lookup_str(raw(), SOLVABLE_MEDIABASE);
    }

    void ObjRepoView::set_channel(raw_str_view str) const
    {
        return repo_set_str(raw(), SOLVABLE_MEDIABASE, str);
    }

    void ObjRepoView::set_channel(const std::string& str) const
    {
        return set_channel(str.c_str());
    }

    auto ObjRepoViewConst::subdir() const -> std::string_view
    {
        return repo_lookup_str(raw(), SOLVABLE_MEDIADIR);
    }

    void ObjRepoView::set_subdir(raw_str_view str) const
    {
        return repo_set_str(raw(), SOLVABLE_MEDIADIR, str);
    }

    void ObjRepoView::set_subdir(const std::string& str) const
    {
        return set_subdir(str.c_str());
    }
}
