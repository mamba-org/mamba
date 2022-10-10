// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UTIL_HPP
#define MAMBA_CORE_UTIL_HPP

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/util_string.hpp"

#include "nlohmann/json.hpp"

#include <array>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <time.h>
#include <vector>
#include <chrono>
#include <optional>

#if defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
#include <iomanip>
#endif

#define MAMBA_EMPTY_SHA "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

namespace mamba
{
#if __APPLE__ || __MACH__
    static constexpr bool on_win = false;
    static constexpr bool on_linux = false;
    static constexpr bool on_mac = true;
#elif __linux__
    static constexpr bool on_win = false;
    static constexpr bool on_linux = true;
    static constexpr bool on_mac = false;
#elif _WIN32
    static constexpr bool on_win = true;
    static constexpr bool on_linux = false;
    static constexpr bool on_mac = false;
#else
#error "no supported OS detected"
#endif

    bool is_package_file(const std::string_view& fn);

    bool lexists(const fs::u8path& p);
    bool lexists(const fs::u8path& p, std::error_code& ec);
    std::vector<fs::u8path> filter_dir(const fs::u8path& dir, const std::string& suffix);
    bool paths_equal(const fs::u8path& lhs, const fs::u8path& rhs);

    std::string read_contents(const fs::u8path& path,
                              std::ios::openmode mode = std::ios::in | std::ios::binary);
    std::vector<std::string> read_lines(const fs::u8path& path);

    inline void make_executable(const fs::u8path& p)
    {
        fs::permissions(p,
                        fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read
                            | fs::perms::others_exec);
    }

    class TemporaryDirectory
    {
    public:
        TemporaryDirectory();
        ~TemporaryDirectory();

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(TemporaryDirectory&&) = default;

        const fs::u8path& path() const;
        operator fs::u8path();

    private:
        fs::u8path m_path;
    };

    class TemporaryFile
    {
    public:
        TemporaryFile(const std::string& prefix = "mambaf", const std::string& suffix = "");
        ~TemporaryFile();

        TemporaryFile(const TemporaryFile&) = delete;
        TemporaryFile& operator=(const TemporaryFile&) = delete;
        TemporaryFile& operator=(TemporaryFile&&) = default;

        fs::u8path& path();
        operator fs::u8path();

    private:
        fs::u8path m_path;
    };

    const std::size_t MAMBA_LOCK_POS = 21;

    class LockFileOwner;

    class LockFile
    {
    public:
        ~LockFile();

        LockFile(const LockFile&) = delete;
        LockFile& operator=(const LockFile&) = delete;

        LockFile(LockFile&&);
        LockFile& operator=(LockFile&&);

        static std::unique_ptr<LockFile> create_lock(const fs::u8path& path);
        static std::unique_ptr<LockFile> try_lock(const fs::u8path& path) noexcept;

        int fd() const;
        fs::u8path path() const;
        fs::u8path lockfile_path() const;

        std::size_t count_lock_owners() const
        {
            return impl.use_count();
        }

#ifdef _WIN32
        // Using file descriptor on Windows may cause false negative
        static bool is_locked(const fs::u8path& path);
#else
        // Opening a new file descriptor on Unix would clear locks
        static bool is_locked(int fd);
#endif

        static bool is_locked(const LockFile& lockfile)
        {
#ifdef _WIN32
            return is_locked(lockfile.lockfile_path());
#else
            // Opening a new file descriptor on Unix would clear locks
            return is_locked(lockfile.fd());
#endif
        }

    private:
        LockFile(const fs::u8path& path);
        LockFile(const fs::u8path& path, const std::chrono::seconds& timeout);
        std::shared_ptr<LockFileOwner> impl;
    };


    void split_package_extension(const std::string& file,
                                 std::string& name,
                                 std::string& extension);
    fs::u8path strip_package_extension(const std::string& file);

    template <class T>
    inline bool vector_is_prefix(const std::vector<T>& prefix, const std::vector<T>& vec)
    {
        return vec.size() >= prefix.size()
               && prefix.end() == std::mismatch(prefix.begin(), prefix.end(), vec.begin()).first;
    }

    tl::expected<std::string, mamba_error> encode_base64(const std::string_view& input);
    tl::expected<std::string, mamba_error> decode_base64(const std::string_view& input);


    // get the value corresponding to a key in a JSON object and assign it to target
    // if the key is not found, assign default_value to target
    template <typename T>
    void assign_or(const nlohmann::json& j, const char* key, T& target, T default_value)
    {
        if (j.contains(key))
            target = j[key];
        else
            target = default_value;
    }

    std::string quote_for_shell(const std::vector<std::string>& arguments,
                                const std::string& shell = "");

    std::size_t clean_trash_files(const fs::u8path& prefix, bool deep_clean);
    std::size_t remove_or_rename(const fs::u8path& path);

    // Unindent a string literal
    std::string unindent(const char* p);

    std::string prepend(const std::string& p, const char* start, const char* newline = "");

    std::string prepend(const char* p, const char* start, const char* newline = "");

    std::string timestamp(const std::time_t& time);

    std::time_t utc_time_now();

    std::string utc_timestamp_now();

    std::time_t parse_utc_timestamp(const std::string& timestamp, int& error_code) noexcept;

    std::time_t parse_utc_timestamp(const std::string& timestamp);

    std::ofstream open_ofstream(const fs::u8path& path,
                                std::ios::openmode mode = std::ios::out | std::ios::binary);

    std::ifstream open_ifstream(const fs::u8path& path,
                                std::ios::openmode mode = std::ios::in | std::ios::binary);

    bool ensure_comspec_set();
    std::unique_ptr<TemporaryFile> wrap_call(const fs::u8path& root_prefix,
                                             const fs::u8path& prefix,
                                             bool dev_mode,
                                             bool debug_wrapper_scripts,
                                             const std::vector<std::string>& arguments);

    std::tuple<std::vector<std::string>, std::unique_ptr<TemporaryFile>> prepare_wrapped_call(
        const fs::u8path& prefix, const std::vector<std::string>& cmd);

    /// Returns `true` if the filename matches names of files which should be interpreted as YAML.
    /// NOTE: this does not check if the file exists.
    inline bool is_yaml_file_name(const std::string_view filename)
    {
        return ends_with(filename, ".yml") || ends_with(filename, ".yaml");
    }

    std::optional<std::string> proxy_match(const std::string& url);

}  // namespace mamba

#endif  // MAMBA_UTIL_HPP
