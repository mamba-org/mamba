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

    auto ObjRepoViewConst::name() const -> std::string_view
    {
        return repo_name(raw());
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

    void ObjRepoViewConst::write(std::filesystem::path solv_file) const
    {
        auto* fp = std::fopen(solv_file.string().c_str(), "wb");
        if (!fp)
        {
            throw std::system_error(errno, std::generic_category());
        }
        const auto write_res = repo_write(const_cast<::Repo*>(raw()), fp);
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
        repo_empty(raw(), static_cast<int>(reuse_ids));
    }

    void ObjRepoView::read(std::filesystem::path solv_file) const
    {
        auto* fp = std::fopen(solv_file.string().c_str(), "rb");
        if (!fp)
        {
            throw std::system_error(errno, std::generic_category());
        }
        const auto read_res = repo_add_solv(raw(), fp, 0);
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
        return repo_add_solvable(raw());
    }
}
