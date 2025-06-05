#include <iostream>
#include <regex>

#ifndef _WIN32
#include <clocale>
// To find the path of `libmamba`'s library.
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <libproc.h>
#include <mach-o/dyld.h>
#endif
#include <inttypes.h>
#include <limits.h>
#else
#include <atomic>

#include <intrin.h>
#include <io.h>
#include <shlobj_core.h>
#include <windows.h>
// Incomplete header included last
#include <tlhelp32.h>
#include <WinReg.hpp>

#include "mamba/core/context.hpp"
#endif

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <reproc++/run.hpp>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/os_win.hpp"
#include "mamba/util/string.hpp"

#ifdef _WIN32
static_assert(std::is_same_v<mamba::DWORD, ::DWORD>);
#endif

namespace mamba
{

#ifdef _WIN32
    fs::u8path get_hmodule_path(HMODULE hModule)
    {
        std::wstring buffer(MAX_PATH, '\0');
        DWORD new_size = MAX_PATH;
        DWORD size = 0;
        // There's unfortunately no way to know how much space is needed,
        // so we just keep doubling the size of the buffer until the path fits.
        while (true)
        {
            size = GetModuleFileNameW(hModule, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (size == 0)
            {
                throw mamba::mamba_error(
                    "Could find the filename of the module handle. (GetModuleFileNameW failed)",
                    mamba_error_code::internal_failure
                );
            }
            if (size < new_size)
            {
                break;
            }

            new_size *= 2;
            buffer.resize(new_size);
        }
        buffer.resize(size);
        return fs::absolute(buffer);
    }
#endif

    // Heavily inspired by https://github.com/gpakosz/whereami/
    // check their source to add support for other OS
    fs::u8path get_self_exe_path()
    {
#ifdef _WIN32
        HMODULE hModule = NULL;  // references file used to create the calling process
        return get_hmodule_path(hModule);
#elif defined(__APPLE__)
        uint32_t size = PATH_MAX;
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) == -1)
        {
            buffer.reserve(size);
            if (!_NSGetExecutablePath(buffer.data(), &size))
            {
                throw std::runtime_error("Couldn't find location the micromamba executable!");
            }
        }
        return fs::absolute(buffer.data());
#else
#if defined(__sun)
        return fs::read_symlink("/proc/self/path/a.out");
#else
        return fs::read_symlink("/proc/self/exe");
#endif
#endif
    }

    fs::u8path get_libmamba_path()
    {
#ifdef _WIN32
        HMODULE hModule = NULL;
        BOOL ret_code = GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR) get_libmamba_path,
            &hModule
        );
        if (!ret_code)
        {
            throw mamba::mamba_error(
                "Could find libmamba's module handle. (GetModuleHandleExW failed)",
                mamba_error_code::internal_failure
            );
        }
        return get_hmodule_path(hModule);
#else
        fs::u8path libmamba_path;
        Dl_info dl_info;
        if (!dladdr(reinterpret_cast<void*>(get_libmamba_path), &dl_info))
        {
            throw mamba_error(
                "Could not find libmamba's path. (dladdr failed)",
                mamba_error_code::internal_failure
            );
        }
        libmamba_path = dl_info.dli_fname;
        return libmamba_path;
#endif
    }

    bool is_admin()
    {
#ifdef _WIN32
        return IsUserAnAdmin();
#else
        return geteuid() == 0 || getegid() == 0;
#endif
    }

#ifdef _WIN32
    bool run_as_admin(const std::string& exe, const std::string& args)
    {
        SHELLEXECUTEINFO excinfo;

        // I've tried for quite some time and could not get this to work at all
        // excinfo.lpFile="micromamba.exe";
        // excinfo.lpParameters="shell enable-long-paths-support";

        ZeroMemory(&excinfo, sizeof(SHELLEXECUTEINFO));
        excinfo.cbSize = sizeof(SHELLEXECUTEINFO);
        excinfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
        excinfo.lpVerb = "runas";
        excinfo.lpFile = exe.c_str();
        excinfo.lpParameters = args.c_str();
        excinfo.lpDirectory = NULL;
        excinfo.nShow = SW_HIDE;

        if (!ShellExecuteEx(&excinfo))
        {
            LOG_WARNING << "Could not start process as admin.";
            return false;
        }

        // Wait for the process to complete, then get its exit code
        DWORD ExitCode = 0;
        WaitForSingleObject(excinfo.hProcess, INFINITE);
        GetExitCodeProcess(excinfo.hProcess, &ExitCode);
        CloseHandle(excinfo.hProcess);
        if (ExitCode != 0)
        {
            LOG_WARNING << "Process exited with code != 0.";
            return false;
        }
        return true;
    }

    bool enable_long_paths_support(bool force, Palette palette)
    {
        // Needs to be set system-wide & can only be run as admin ...

        const auto win_ver = util::windows_version();
        LOG_DEBUG << fmt::format(
            "Windows version : {}",
            win_ver ? win_ver.value() : win_ver.error().message
        );

        static constexpr auto error_message_wrong_version = "Not setting long path registry key;"
                                                            "Windows version must be at least 10 with the fall 2016 \"Anniversary update\" or newer.";

        if (!win_ver.has_value())
        {
            LOG_WARNING << "failed to acquire Windows version - " << error_message_wrong_version;
            return false;
        }


        auto split_out = util::split(win_ver.value(), ".");
        if (!(split_out.size() >= 3 && std::stoull(split_out[0]) >= 10
              && std::stoull(split_out[2]) >= 14352))
        {
            LOG_WARNING << "Windows version found:" << win_ver.value() << " - "
                        << error_message_wrong_version;
            return false;
        }

        winreg::RegKey key;
        key.Open(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\FileSystem", KEY_QUERY_VALUE);
        DWORD prev_value;
        try
        {
            prev_value = key.GetDwordValue(L"LongPathsEnabled");
        }
        catch (const winreg::RegException& /*e*/)
        {
            LOG_INFO << "No LongPathsEnabled key detected. (Windows version = " << win_ver.value()
                     << ")";
            return false;
        }

        if (prev_value == 1)
        {
            auto out = Console::stream();
            fmt::print(
                out,
                "{} (Windows version = {})",
                fmt::styled("Windows long-path support already enabled.", palette.ignored),
                win_ver.value()
            );
            return true;
        }

        if (force || is_admin())
        {
            winreg::RegKey key_for_write(
                HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\Control\\FileSystem"
            );
            key_for_write.SetDwordValue(L"LongPathsEnabled", 1);
        }
        else
        {
            if (Console::prompt("Enter admin mode to enable long paths support?", 'n'))
            {
                if (!run_as_admin(
                        "reg.exe",
                        "ADD HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\FileSystem /v LongPathsEnabled /d 1 /t REG_DWORD /f"
                    ))
                {
                    return false;
                }
            }
            else
            {
                LOG_WARNING << "Did not enable long paths support.";
                return false;
            }
        }

        prev_value = key.GetDwordValue(L"LongPathsEnabled");
        if (prev_value == 1)
        {
            auto out = Console::stream();
            fmt::print(out, "{}", fmt::styled("Windows long-path support enabled.", palette.success));
            return true;
        }
        LOG_WARNING << "Changing registry value did not succeed.";
        return false;
    }

    DWORD getppid()
    {
        HANDLE hSnapshot;
        PROCESSENTRY32 pe32;
        DWORD ppid = 0, pid = GetCurrentProcessId();

        hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        __try
        {
            if (hSnapshot == INVALID_HANDLE_VALUE)
            {
                __leave;
            }

            ZeroMemory(&pe32, sizeof(pe32));
            pe32.dwSize = sizeof(pe32);
            if (!Process32First(hSnapshot, &pe32))
            {
                __leave;
            }

            do
            {
                if (pe32.th32ProcessID == pid)
                {
                    ppid = pe32.th32ParentProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        __finally
        {
            if (hSnapshot != INVALID_HANDLE_VALUE)
            {
                CloseHandle(hSnapshot);
            }
        }
        return ppid;
    }

    std::string get_process_name_by_pid(DWORD processId)
    {
        std::string ret;
        HANDLE handle = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            processId /* This is the PID, you can find one from windows task manager */
        );
        if (handle)
        {
            DWORD buffSize = 1024;
            CHAR buffer[1024];
            if (QueryFullProcessImageNameA(handle, 0, buffer, &buffSize))
            {
                ret = buffer;
            }
            else
            {
                printf("Error GetModuleBaseNameA : %lu", GetLastError());
            }
            CloseHandle(handle);
        }
        else
        {
            printf("Error OpenProcess : %lu", GetLastError());
        }
        return ret;
    }
#elif defined(__APPLE__)
    std::string get_process_name_by_pid(const int pid)
    {
        std::string ret;
        char name[1024];
        proc_name(pid, name, sizeof(name));
        ret = name;

        return ret;
    }
#elif defined(__linux__)
    std::string get_process_name_by_pid(const int pid)
    {
        std::ifstream f(util::concat("/proc/", std::to_string(pid), "/status"));
        if (f.good())
        {
            std::string l;
            std::getline(f, l);
            return l;
        }
        return "";
    }
#endif

#ifdef _WIN32
    namespace
    {
        static std::atomic<int> init_console_cp(0);
        static std::atomic<int> init_console_output_cp(0);
        static std::atomic<bool> init_console_initialized(false);
    }
#endif

    // init console to make sure UTF8 is properly activated
    void init_console()
    {
#ifdef _WIN32
        init_console_cp = GetConsoleCP();
        init_console_output_cp = GetConsoleOutputCP();
        init_console_initialized = true;

        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        // Enable buffering to prevent VS from chopping up UTF-8 byte sequences
        setvbuf(stdout, nullptr, _IOFBF, 1000);
#else
        static constexpr const char* utf8_locales[] = {
            "C.UTF-8",
            "POSIX.UTF-8",
            "en_US.UTF-8",
        };

        for (const char* utf8_locale : utf8_locales)
        {
            if (::setlocale(LC_ALL, utf8_locale))
            {
                ::setenv("LC_ALL", utf8_locale, true);
                break;
            }
        }
#endif
    }

    void reset_console()
    {
#ifdef _WIN32
        if (init_console_initialized)
        {
            SetConsoleCP(init_console_cp);
            SetConsoleOutputCP(init_console_output_cp);
        }
#endif
    }

    /* From https://github.com/ikalnytskyi/termcolor
     *
     * copyright: (c) 2013 by Ihor Kalnytskyi.
     * license: BSD
     * */
    bool is_atty(const std::ostream& stream)
    {
        FILE* const std_stream = [](const std::ostream& lstream) -> FILE*
        {
            if (&lstream == &std::cout)
            {
                return stdout;
            }
            else if ((&lstream == &std::cerr) || (&lstream == &std::clog))
            {
                return stderr;
            }
            else
            {
                return nullptr;
            }
        }(stream);

        // Unfortunately, fileno() ends with segmentation fault
        // if invalid file descriptor is passed. So we need to
        // handle this case gracefully and assume it's not a tty
        // if standard stream is not detected, and 0 is returned.
        if (!std_stream)
        {
            return false;
        }

#ifdef _WIN32
        return ::_isatty(_fileno(std_stream));
#else
        return ::isatty(fileno(std_stream));
#endif
    }

    ConsoleFeatures get_console_features()
    {
        ConsoleFeatures features;
#ifndef _WIN32
        features.virtual_terminal_processing = true;
        features.true_colors = true;
#else
        DWORD console_mode;
        bool res = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &console_mode);
        features.virtual_terminal_processing = res
                                               && console_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        features.true_colors = false;

        if (auto version = util::windows_version())
        {
            auto split_out = util::split(version.value(), '.');
            if (split_out.size() >= 3 && std::stoull(split_out[0]) >= 10
                && std::stoull(split_out[2]) >= 15063)
            {
                features.true_colors = true;
            }
        }
#endif
        return features;
    }

    int get_console_width()
    {
#ifndef _WIN32
        struct winsize w;
        ioctl(0, TIOCGWINSZ, &w);
        return w.ws_col;
#else

        CONSOLE_SCREEN_BUFFER_INFO coninfo;
        auto res = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
        return coninfo.dwSize.X;
#endif
    }

    int get_console_height()
    {
#ifndef _WIN32
        struct winsize w;
        ioctl(0, TIOCGWINSZ, &w);
        return w.ws_row;
#else

        CONSOLE_SCREEN_BUFFER_INFO coninfo;
        auto res = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
        return coninfo.srWindow.Bottom - coninfo.srWindow.Top + 1;
#endif
    }

    void codesign(const fs::u8path& path, bool verbose)
    {
        reproc::options options;
        options.env.behavior = reproc::env::empty;
        if (!verbose)
        {
            reproc::redirect silence;
            silence.type = reproc::redirect::discard;
            options.redirect.out = silence;
            options.redirect.err = silence;
        }

        const std::vector<std::string> cmd = { "/usr/bin/codesign", "-s", "-", "-f", path.string() };
        auto [status, ec] = reproc::run(cmd, options);
        if (ec)
        {
            throw std::runtime_error(std::string("Could not codesign executable: ") + ec.message());
        }
    }
}
