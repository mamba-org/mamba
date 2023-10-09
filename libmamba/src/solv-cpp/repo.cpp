// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <exception>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/repo_solv.h>
#include <solv/repo_write.h>
#include <solv/solvable.h>
extern "C"  // Incomplete header in libsolv 0.7.23
{
#include <solv/conda.h>
#include <solv/repo_conda.h>
}

#include "mamba/fs/filesystem.hpp"
#include "solv-cpp/repo.hpp"

namespace mamba::solv
{
    /****************************************
     *  Implementation of ConstObjRepoView  *
     ****************************************/

    auto ObjRepoViewConst::of_solvable(ObjSolvableViewConst s) -> ObjRepoViewConst
    {
        assert(s.raw()->repo != nullptr);
        return ObjRepoViewConst(*(s.raw()->repo));
    }

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
        class CFile
        {
        public:

            /**
             * Open a file with C API.
             *
             * @param path must hae filesystem default encoding.
             */
            static auto open(const mamba::fs::u8path& path, const char* mode) -> CFile;

            /**
             * The destructor will flush and close the file descriptor.
             *
             * Like ``std::fstream``, exceptions are ignored.
             * Explicitly call @ref close to get the exception.
             */
            ~CFile();

            void close();

            auto raw() noexcept -> std::FILE*;

        private:

            CFile(std::FILE* ptr, std::string name);

            std::FILE* m_ptr = nullptr;
            std::string m_name = {};
        };

        CFile::CFile(std::FILE* ptr, std::string name)
            : m_ptr{ ptr }
            , m_name{ name }
        {
        }

        CFile::~CFile()
        {
            try
            {
                close();
            }
            catch (const std::exception& e)
            {
                std::cerr << "Developer error: "
                             "uncaught exception in CFile::~CFile, "
                             "explicitly call CFile::close to handle exception.\n"
                          << e.what();
            }
        }

        auto CFile::open(const mamba::fs::u8path& path, const char* mode) -> CFile
        {
#ifdef _WIN32
            // Mode MUST be an ASCII string
            const auto wmode = std::wstring(mode, mode + std::strlen(mode));
            auto ptr = ::_wfsopen(path.wstring().c_str(), wmode.c_str(), _SH_DENYNO);
            if (ptr == nullptr)
            {
                throw std::system_error(GetLastError(), std::generic_category());
            }
            return { ptr, path.string() };
#else
            std::string name = path.string();
            std::FILE* ptr = std::fopen(name.c_str(), mode);
            if (ptr == nullptr)
            {
                throw std::system_error(errno, std::generic_category());
            }
            return { ptr, std::move(name) };
#endif
        }

        void CFile::close()
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

        auto CFile::raw() noexcept -> std::FILE*
        {
            return m_ptr;
        }
    }

    void ObjRepoViewConst::write(const mamba::fs::u8path& solv_file) const
    {
        auto file = CFile::open(solv_file, "wb");
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

    void ObjRepoView::read(const mamba::fs::u8path& solv_file) const
    {
        auto file = CFile::open(solv_file, "rb");
        const auto read_res = ::repo_add_solv(raw(), file.raw(), 0);
        if (read_res != 0)
        {
            // TODO(C++20) fmt::format
            auto ss = std::stringstream();
            ss << "Unable to read repo solv file '" << name() << '\'';
            if (const char* str = ::pool_errstr(raw()->pool))
            {
                ss << ", error was: " << str;
            }
            throw std::runtime_error(ss.str());
        }
    }

    void
    ObjRepoView::legacy_read_conda_repodata(const mamba::fs::u8path& repodata_file, int flags) const
    {
        auto file = CFile::open(repodata_file, "rb");
        const auto res = ::repo_add_conda(raw(), file.raw(), flags);
        if (res != 0)
        {
            // TODO(C++20) fmt::format
            auto ss = std::stringstream();
            ss << "Unable to read repodata JSON file '" << name() << '\'';
            if (const char* str = ::pool_errstr(raw()->pool))
            {
                ss << ", error was: " << str;
            }
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

        // Can only read key/value on solvable, but the special SOLVID_META is used for a fake
        // solvable representing the repo.
        // The key used does not really matter so we can (ab)use any key that does not have
        // special meaning

        auto repo_lookup_str(const ::Repo* repo, ::Id key) -> std::string_view
        {
            return ptr_to_strview(::repo_lookup_str(const_cast<::Repo*>(repo), SOLVID_META, key));
        }

        void repo_set_str(::Repo* repo, ::Id key, const char* str)
        {
            ::repo_set_str(repo, SOLVID_META, key, str);
        }

        auto repo_lookup_num(const ::Repo* repo, ::Id key) -> std::size_t
        {
            return ::repo_lookup_num(const_cast<::Repo*>(repo), SOLVID_META, key, 0);
        }

        void repo_set_num(::Repo* repo, ::Id key, std::size_t n)
        {
            ::repo_set_num(repo, SOLVID_META, key, n);
        }

        auto repo_lookup_bool(const ::Repo* repo, ::Id key) -> bool
        {
            return repo_lookup_num(repo, key) != 0;
        }

        void repo_set_bool(::Repo* repo, ::Id key, bool b)
        {
            repo_set_num(repo, key, b);
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

    namespace
    {
        // This does modify the pool but does not impact our use
        auto etag_key(const ::Repo* repo) -> StringId
        {
            return ::pool_str2id(repo->pool, "repository:etag", /* create= */ true);
        }
    }

    auto ObjRepoViewConst::etag() const -> std::string_view
    {
        return repo_lookup_str(raw(), etag_key(raw()));
    }

    void ObjRepoView::set_etag(raw_str_view str) const
    {
        return repo_set_str(raw(), etag_key(raw()), str);
    }

    void ObjRepoView::set_etag(const std::string& str) const
    {
        return set_etag(str.c_str());
    }

    namespace
    {
        // This does modify the pool but does not impact our use
        auto mod_key(const ::Repo* repo) -> StringId
        {
            return ::pool_str2id(repo->pool, "repository:mod", /* create= */ true);
        }
    }

    auto ObjRepoViewConst::mod() const -> std::string_view
    {
        return repo_lookup_str(raw(), mod_key(raw()));
    }

    void ObjRepoView::set_mod(raw_str_view str) const
    {
        return repo_set_str(raw(), mod_key(raw()), str);
    }

    void ObjRepoView::set_mod(const std::string& str) const
    {
        return set_mod(str.c_str());
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

    namespace
    {
        // This does modify the pool but does not impact our use
        auto pip_added_key(const ::Repo* repo) -> StringId
        {
            return ::pool_str2id(repo->pool, "repository:pip_added", /* create= */ true);
        }
    }

    auto ObjRepoViewConst::pip_added() const -> bool
    {
        return repo_lookup_bool(raw(), pip_added_key(raw()));
    }

    void ObjRepoView::set_pip_added(bool b) const
    {
        return repo_set_bool(raw(), pip_added_key(raw()), b);
    }

    auto ObjRepoViewConst::tool_version() const -> std::string_view
    {
        return repo_lookup_str(raw(), REPOSITORY_TOOLVERSION);
    }

    void ObjRepoView::set_tool_version(raw_str_view str) const
    {
        return repo_set_str(raw(), REPOSITORY_TOOLVERSION, str);
    }

    void ObjRepoView::set_tool_version(const std::string& str) const
    {
        return set_tool_version(str.c_str());
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
