// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "mamba/util/cfile.hpp"

namespace mamba::util
{
    namespace
    {
        void try_close_impl(std::FILE* ptr, std::error_code& ec) noexcept
        {
            if (ptr)
            {
                const auto close_res = std::fclose(ptr);  // This flush too
                if (close_res != 0)
                {
                    ec = std::make_error_code(std::errc::io_error);
                }
            }
        }
    }

    void CFile::FileClose::operator()(std::FILE* ptr)
    {
        auto ec = std::error_code();
        try_close_impl(ptr, ec);
        if (ec)
        {
            std::cerr << "Developer error: error closing file in CFile::~CFile, "
                         "explicitly call CFile::try_close to handle error.\n";
        }
    }

    CFile::CFile(std::FILE* ptr)
        : m_ptr{ ptr }
    {
    }

    CFile::~CFile() = default;

    auto CFile::try_open(const mamba::fs::u8path& path, const char* mode, std::error_code& ec) -> CFile
    {
#ifdef _WIN32
        // Mode MUST be an ASCII string
        const auto wmode = std::wstring(mode, mode + std::strlen(mode));
        auto ptr = ::_wfsopen(path.wstring().c_str(), wmode.c_str(), _SH_DENYNO);
        if (ptr == nullptr)
        {
            ec = std::error_code(GetLastError(), std::generic_category());
        }
        return CFile{ ptr };
#else
        std::string name = path.string();
        std::FILE* ptr = std::fopen(name.c_str(), mode);
        if (ptr == nullptr)
        {
            ec = std::error_code(errno, std::generic_category());
        }
        return CFile{ ptr };
#endif
    }

    auto CFile::try_open(const mamba::fs::u8path& path, const char* mode)
        -> tl::expected<CFile, std::error_code>
    {
        auto io_error = std::error_code();
        auto file_ptr = util::CFile::try_open(path, mode, io_error);
        if (io_error)
        {
            return tl::unexpected(io_error);
        }
        return { std::move(file_ptr) };
    }

    void CFile::try_close(std::error_code& ec) noexcept
    {
        try_close_impl(m_ptr.get(), ec);
        [[maybe_unused]] auto raw = m_ptr.release();  // No need to call dtor anymore
    }

    auto CFile::try_close() noexcept -> tl::expected<void, std::error_code>
    {
        auto io_error = std::error_code();
        try_close(io_error);
        if (io_error)
        {
            return tl::unexpected(io_error);
        }
        return {};
    }

    auto CFile::raw() noexcept -> std::FILE*
    {
        return m_ptr.get();
    }
}
