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

    ObjRepoViewConst::ObjRepoViewConst(const ::Repo& repo) noexcept
        : m_repo{ &repo }
    {
    }

    ObjRepoViewConst::~ObjRepoViewConst() noexcept
    {
        m_repo = nullptr;
    }

    auto ObjRepoViewConst::raw() const -> const ::Repo*
    {
        return m_repo;
    }

    auto ObjRepoViewConst::id() const -> RepoId
    {
        return raw()->repoid;
    }

    auto ObjRepoViewConst::solvable_count() const -> std::size_t
    {
        assert(raw()->nsolvables >= 0);
        return static_cast<std::size_t>(raw()->nsolvables);
    }

    namespace
    {
        auto get_solvable_ptr(const ::Repo* repo, SolvableId id) -> ::Solvable*
        {
            if ((id >= repo->start) && (id < repo->end))
            {
                if (Solvable* const s = ::pool_id2solvable(repo->pool, id); s != nullptr)
                {
                    if (s->repo == repo)
                    {
                        return s;
                    }
                }
            }
            return nullptr;
        }
    }

    auto ObjRepoViewConst::has_solvable(SolvableId id) const -> bool
    {
        return get_solvable_ptr(raw(), id) != nullptr;
    }

    auto ObjRepoViewConst::get_solvable(SolvableId id) const -> std::optional<ObjSolvableViewConst>
    {
        if (const ::Solvable* s = get_solvable_ptr(raw(), id))
        {
            return { ObjSolvableViewConst{ *s } };
        }
        return std::nullopt;
    }

    namespace
    {
        class FilePtr
        {
        public:

            /**
             * Open a file with C API.
             *
             * @param path must hae filesystem default encoding.
             */
            static auto open(std::filesystem::path path, const char* mode) -> FilePtr;

            ~FilePtr() noexcept(false);

            auto raw() noexcept -> std::FILE*;
            auto raw() const noexcept -> const std::FILE*;

        private:

            FilePtr(std::FILE* ptr, std::string name);

            std::FILE* m_ptr = nullptr;
            std::string m_name = {};
        };

        FilePtr::FilePtr(std::FILE* ptr, std::string name)
            : m_ptr{ ptr }
            , m_name{ name }
        {
        }

        auto FilePtr::open(std::filesystem::path path, const char* mode) -> FilePtr
        {
            std::string name = path;
            std::FILE* ptr = std::fopen(name.c_str(), mode);
            if (ptr == nullptr)
            {
                throw std::system_error(errno, std::generic_category());
            }
            return { ptr, std::move(name) };
        }

        FilePtr::~FilePtr() noexcept(false)
        {
            const auto close_res = std::fclose(m_ptr);  // This flush too
            if (close_res != 0)
            {
                // TODO(C++20) fmt::format
                auto ss = std::stringstream();
                ss << "Unable to close file " << m_name;
                throw std::runtime_error(ss.str());
            }
        }

        auto FilePtr::raw() noexcept -> std::FILE*
        {
            return m_ptr;
        }

        auto FilePtr::raw() const noexcept -> const std::FILE*
        {
            return m_ptr;
        }
    }

    void ObjRepoViewConst::write(std::filesystem::path solv_file) const
    {
        auto file = FilePtr::open(solv_file, "wb");
        const auto write_res = ::repo_write(const_cast<::Repo*>(raw()), file.raw());
        if (write_res != 0)
        {
            // TODO(C++20) fmt::format
            auto ss = std::stringstream();
            ss << "Unable to write repo '" << name() << "' to file";
            throw std::runtime_error(ss.str());
        }
    }

    /***********************************
     *  Implementation of ObjRepoView  *
     ***********************************/

    ObjRepoView::ObjRepoView(::Repo& repo) noexcept
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
        auto file = FilePtr::open(solv_file, "rb");
        const auto read_res = ::repo_add_solv(raw(), file.raw(), 0);
        if (read_res != 0)
        {
            // TODO(C++20) fmt::format
            auto ss = std::stringstream();
            ss << "Unable to read repo solv file '" << name() << '\'';
            throw std::runtime_error(ss.str());
        }
    }

    auto ObjRepoView::add_solvable() const -> std::pair<SolvableId, ObjSolvableView>
    {
        const SolvableId id = ::repo_add_solvable(raw());
        return {
            id,
            get_solvable(id).value()  // Safe because we just added the solvable
        };
    }

    auto ObjRepoView::get_solvable(SolvableId id) const -> std::optional<ObjSolvableView>
    {
        if (::Solvable* s = get_solvable_ptr(raw(), id))
        {
            return { ObjSolvableView{ *s } };
        }
        return std::nullopt;
    }

    auto ObjRepoView::remove_solvable(SolvableId id, bool reuse_id) const -> bool
    {
        if (has_solvable(id))
        {
            ::repo_free_solvable(raw(), id, reuse_id);
            return true;
        }
        return false;
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
                return {};
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
