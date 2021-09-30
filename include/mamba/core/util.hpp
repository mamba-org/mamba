// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UTIL_HPP
#define MAMBA_CORE_UTIL_HPP

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/output.hpp"

#include "nlohmann/json.hpp"

#include <array>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <time.h>
#include <vector>
#include <regex>
#include <chrono>


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

    // usernames on anaconda.org can have a underscore, which influences the
    // first two characters
    const static std::regex token_re("/t/([a-zA-Z0-9-_]{0,2}[a-zA-Z0-9-]*)");
    const static std::regex http_basicauth_re("://([^\\s]+):([^\\s]+)@");

    class mamba_error : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    bool is_package_file(const std::string_view& fn);

    void to_human_readable_filesize(std::ostream& o, double bytes, std::size_t precision = 0);
    bool lexists(const fs::path& p);
    std::vector<fs::path> filter_dir(const fs::path& dir, const std::string& suffix);
    bool paths_equal(const fs::path& lhs, const fs::path& rhs);

    std::string read_contents(const fs::path& path,
                              std::ios::openmode mode = std::ios::in | std::ios::binary);
    std::vector<std::string> read_lines(const fs::path& path);

    inline void make_executable(const fs::path& p)
    {
        fs::permissions(p,
                        fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read
                            | fs::perms::others_exec);
    }

    template <typename T = std::mt19937>
    inline auto random_generator() -> T
    {
        auto constexpr seed_bits = sizeof(typename T::result_type) * T::state_size;
        auto constexpr seed_len
            = seed_bits / std::numeric_limits<std::seed_seq::result_type>::digits;
        auto seed = std::array<std::seed_seq::result_type, seed_len>{};
        auto dev = std::random_device{};
        std::generate_n(begin(seed), seed_len, std::ref(dev));
        auto seed_seq = std::seed_seq(begin(seed), end(seed));
        return T{ seed_seq };
    }

    inline std::string generate_random_alphanumeric_string(std::size_t len)
    {
        static constexpr auto chars = "0123456789"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "abcdefghijklmnopqrstuvwxyz";
        thread_local auto rng = random_generator<std::mt19937>();

        auto dist = std::uniform_int_distribution{ {}, std::strlen(chars) - 1 };
        auto result = std::string(len, '\0');
        std::generate_n(begin(result), len, [&]() { return chars[dist(rng)]; });
        return result;
    }

    class TemporaryDirectory
    {
    public:
        TemporaryDirectory();
        ~TemporaryDirectory();

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(TemporaryDirectory&&) = default;

        fs::path& path();
        operator fs::path();

    private:
        fs::path m_path;
    };

    class TemporaryFile
    {
    public:
        TemporaryFile(const std::string& prefix = "mambaf", const std::string& suffix = "");
        ~TemporaryFile();

        TemporaryFile(const TemporaryFile&) = delete;
        TemporaryFile& operator=(const TemporaryFile&) = delete;
        TemporaryFile& operator=(TemporaryFile&&) = default;

        fs::path& path();
        operator fs::path();

    private:
        fs::path m_path;
    };

    class Lock;

    class LockFile
    {
    public:
        // LockFile(const fs::path& path);
        LockFile(const fs::path& path, const std::chrono::seconds& timeout);
        ~LockFile();

        int fd() const;

#ifdef _WIN32
        // Using file descriptor on Windows may cause false negative
        static bool is_locked(const fs::path& path);
#else
        // Opening a new file descriptor on Unix would clear locks
        static bool is_locked(int fd);
#endif
        static int read_pid(int fd);

    private:
        fs::path m_path;
        std::chrono::seconds m_timeout;
        int m_fd = -1;

        bool set_lock(bool blocking) const;

        int read_pid() const;
        bool write_pid(int pid) const;
        bool locked() const;
        bool lock(int pid, bool blocking) const;
        bool unlock() const;
        int close_fd();
        void remove() noexcept;

        friend class Lock;
    };

    class Lock
    {
    public:
        Lock(const fs::path& path);
        ~Lock();

        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
        Lock& operator=(Lock&&) = default;

        bool locked() const;
        int fd() const;
        fs::path path() const;

    private:
        fs::path m_path;
        fs::path m_lock;
        std::unique_ptr<LockFile> p_lock_file;
        bool m_locked;

#if defined(__APPLE__) or defined(__linux__)
        pid_t m_pid;
#else
        int m_pid;
#endif

        bool try_lock();
        bool lock();
    };

    /*************************
     * utils for std::string *
     *************************/

    inline const char* check_char(const char* ptr)
    {
        return ptr ? ptr : "";
    }

    constexpr const char* WHITESPACES(" \r\n\t\f\v");

    bool starts_with(const std::string_view& str, const std::string_view& prefix);
    bool ends_with(const std::string_view& str, const std::string_view& suffix);
    bool contains(const std::string_view& str, const std::string_view& sub_str);

    bool any_starts_with(const std::vector<std::string_view>& str, const std::string_view& prefix);

    bool starts_with_any(const std::string_view& str, const std::vector<std::string_view>& prefix);

    std::string_view strip(const std::string_view& input);
    std::string_view lstrip(const std::string_view& input);
    std::string_view rstrip(const std::string_view& input);

    std::string_view strip(const std::string_view& input, const std::string_view& chars);
    std::string_view lstrip(const std::string_view& input, const std::string_view& chars);
    std::string_view rstrip(const std::string_view& input, const std::string_view& chars);

    std::vector<std::string> split(const std::string_view& input,
                                   const std::string_view& sep,
                                   std::size_t max_split = SIZE_MAX);

    std::vector<std::string> rsplit(const std::string_view& input,
                                    const std::string_view& sep,
                                    std::size_t max_split = SIZE_MAX);

    void split_package_extension(const std::string& file,
                                 std::string& name,
                                 std::string& extension);
    fs::path strip_package_extension(const std::string& file);

    template <class S>
    inline std::string join(const char* j, const S& container)
    {
        if (container.empty())
            return "";
        std::string result = container[0];
        for (std::size_t i = 1; i < container.size(); ++i)
        {
            result += j;
            result += container[i];
        }
        return result;
    }

    void replace_all(std::string& data, const std::string& search, const std::string& replace);

    void replace_all(std::wstring& data, const std::wstring& search, const std::wstring& replace);

    // Note: this function only works for non-unicode!
    std::string to_upper(const std::string_view& input);
    std::string to_lower(const std::string_view& input);

    namespace concat_impl
    {
        template <class T>
        inline void concat_foreach(std::string& result, const T& rhs)
        {
            result += rhs;
        }

        template <class T, class... Rest>
        inline void concat_foreach(std::string& result, const T& rhs, const Rest&... rest)
        {
            result += rhs;
            concat_foreach(result, rest...);
        }

        struct sizer
        {
            inline sizer(const char* s)
                : size(strlen(s))
            {
            }

            inline sizer(const char s)
                : size(1)
            {
            }

            template <class T>
            inline sizer(T& s)
                : size(s.size())
            {
            }

            std::size_t size;
        };
    }  // namespace concat_impl

    template <typename... Args>
    inline std::string concat(const Args&... args)
    {
        size_t len = 0;
        for (auto s : std::initializer_list<concat_impl::sizer>{ args... })
            len += s.size;

        std::string result;
        result.reserve(len);
        concat_impl::concat_foreach(result, args...);
        return result;
    }

    template <class B>
    inline std::string hex_string(const B& buffer, std::size_t size)
    {
        std::ostringstream oss;
        oss << std::hex;
        for (std::size_t i = 0; i < size; ++i)
        {
            oss << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
        }
        return oss.str();
    }

    template <class B>
    inline std::string hex_string(const B& buffer)
    {
        return hex_string(buffer, buffer.size());
    }

    template <class B>
    std::vector<unsigned char> hex_to_bytes(const B& buffer, std::size_t size) noexcept
    {
        std::vector<unsigned char> res;
        if (size % 2 != 0)
            return res;

        std::string extract;
        for (auto pos = buffer.cbegin(); pos < buffer.cend(); pos += 2)
        {
            extract.assign(pos, pos + 2);
            res.push_back(std::stoi(extract, nullptr, 16));
        }
        return res;
    }

    template <class B>
    std::vector<unsigned char> hex_to_bytes(const B& buffer) noexcept
    {
        return hex_to_bytes(buffer, buffer.size());
    }

    template <size_t S, class B>
    std::array<unsigned char, S> hex_to_bytes(const B& buffer, int& error_code) noexcept
    {
        std::array<unsigned char, S> res{};
        if (buffer.size() != (S * 2))
        {
            LOG_DEBUG << "Wrong size for hexadecimal buffer, expected " << S * 2 << " but is "
                      << buffer.size();
            error_code = 1;
            return res;
        }

        std::string extract;
        std::size_t i = 0;
        for (auto pos = buffer.cbegin(); pos < buffer.cend(); pos += 2)
        {
            extract.assign(pos, pos + 2);
            res[i] = std::stoi(extract, nullptr, 16);
            ++i;
        }
        return res;
    }

    template <size_t S, class B>
    std::array<unsigned char, S> hex_to_bytes(const B& buffer) noexcept
    {
        int ec;
        return hex_to_bytes<S>(buffer, ec);
    }

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

    void remove_or_rename(const fs::path& path);

    // Unindent a string literal
    std::string unindent(const char* p);

    std::string prepend(const std::string& p, const char* start, const char* newline = "");

    std::string prepend(const char* p, const char* start, const char* newline = "");

    std::string timestamp(const std::time_t& time);

    std::time_t utc_time_now();

    std::string utc_timestamp_now();

    std::time_t parse_utc_timestamp(const std::string& timestamp, int& error_code) noexcept;

    std::time_t parse_utc_timestamp(const std::string& timestamp);

}  // namespace mamba

#endif  // MAMBA_UTIL_HPP
