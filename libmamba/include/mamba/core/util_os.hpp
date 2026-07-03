// Copyright (c) 2019-2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UTIL_OS_HPP
#define MAMBA_CORE_UTIL_OS_HPP

#include <iosfwd>
#include <string>

#include "mamba/core/fsutil.hpp"
#include "mamba/core/palette.hpp"

namespace mamba
{
#ifdef _WIN32
    // Intention is to avoid including `Windows.h`, while still using the basic Windows API types.
    using DWORD = unsigned long;
#endif

    bool is_admin();
    fs::u8path get_self_exe_path();
    fs::u8path get_libmamba_path();

    using PID =
#ifdef _WIN32
        DWORD
#else
        int
#endif
        ;

    std::string get_process_name_by_pid(const PID pid);
#ifdef _WIN32
    PID getppid();
#endif

    void run_as_admin(const std::string& args);

#ifdef _WIN32
    bool enable_long_paths_support(bool force, Palette palette = Palette::no_color());

    /**
     * Return whether the current process can use paths longer than MAX_PATH.
     *
     * Requires Windows 10 version 1607 or later, ``LongPathsEnabled`` in the registry, and a
     * ``longPathAware`` application manifest (micromamba ships ``longpath.manifest`` for this).
     * See Microsoft's documentation:
     * https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry#enable-long-paths-in-windows-10-version-1607-and-later
     */
    [[nodiscard]] auto are_long_paths_enabled() -> bool;

    /**
     * Human-readable diagnostics for each unmet long-path requirement; empty string when enabled.
     */
    [[nodiscard]] auto long_paths_support_diagnostic() -> std::string;

    /**
     * Log a long-path diagnostic when @p error is a path-length related filesystem error and long
     * paths are disabled.
     */
    void log_long_paths_support_hint_if_relevant(const std::exception& error);
#endif

    void init_console();
    void reset_console();

    /* Test whether a given `std::ostream` object refers to a terminal. */
    bool is_atty(const std::ostream& stream);

    struct ConsoleFeatures
    {
        bool virtual_terminal_processing, true_colors;
    };

    ConsoleFeatures get_console_features();
    int get_console_width();
    int get_console_height();

    void codesign(const fs::u8path& path, bool verbose = false);
}

#endif
