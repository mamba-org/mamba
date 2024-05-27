// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_CFILE_HPP
#define MAMBA_UTIL_CFILE_HPP

#include <cstdio>
#include <memory>
#include <system_error>

#include <tl/expected.hpp>

#include "mamba/fs/filesystem.hpp"

namespace mamba::util
{
    class CFile
    {
    public:

        /**
         * Open a file with C API.
         *
         * In case of error, set the error code @p ec.
         *
         * @param path must have filesystem default encoding.
         */
        static auto try_open(  //
            const fs::u8path& path,
            const char* mode,
            std::error_code& ec
        ) -> CFile;

        static auto try_open(  //
            const fs::u8path& path,
            const char* mode
        ) -> tl::expected<CFile, std::error_code>;

        CFile(CFile&&) = default;
        auto operator=(CFile&&) -> CFile& = default;

        /**
         * The destructor will flush and close the file descriptor.
         *
         * Like ``std::fstream``, exceptions are ignored.
         * Explicitly call @ref close to get the exception.
         */
        ~CFile();

        void try_close(std::error_code& ec) noexcept;
        [[nodiscard]] auto try_close() noexcept -> tl::expected<void, std::error_code>;

        auto raw() noexcept -> std::FILE*;

    private:

        struct FileClose
        {
            void operator()(std::FILE* ptr);
        };

        std::unique_ptr<std::FILE, FileClose> m_ptr = nullptr;

        explicit CFile(std::FILE* ptr);
    };
}
#endif
