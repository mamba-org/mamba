#ifndef MAMBA_UTIL_HPP
#define MAMBA_UTIL_HPP

#include <stdexcept>
#include <string_view>
#include <string>
#include <random>
#include <array>

#include "nlohmann/json.hpp"
#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

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

    std::string get_file_contents(const fs::path& path, std::ios::openmode mode = std::ios::in);

    inline void make_executable(const fs::path& p)
    {
        fs::permissions(p, fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec);
    }

    template <typename T = std::mt19937>
    inline auto random_generator() -> T
    {
        auto constexpr seed_bits = sizeof(typename T::result_type) * T::state_size;
        auto constexpr seed_len = seed_bits / std::numeric_limits<std::seed_seq::result_type>::digits;
        auto seed = std::array<std::seed_seq::result_type, seed_len>{};
        auto dev = std::random_device{};
        std::generate_n(begin(seed), seed_len, std::ref(dev));
        auto seed_seq = std::seed_seq(begin(seed), end(seed));
        return T{seed_seq};
    }

    inline std::string generate_random_alphanumeric_string(std::size_t len)
    {
        static constexpr auto chars =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        thread_local auto rng = random_generator<std::mt19937>();

        auto dist = std::uniform_int_distribution{{}, std::strlen(chars)};
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

    /*************************
     * utils for std::string *
     *************************/

    constexpr const char* WHITESPACES(" \r\n\t\f\v");

    bool starts_with(const std::string_view& str, const std::string_view& prefix);
    bool ends_with(const std::string_view& str, const std::string_view& suffix);

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

    template <class S>
    inline std::string join(const char* j, const S& container)
    {
        if (container.empty()) return "";
        std::string result = container[0];
        for (std::size_t i = 1; i < container.size(); ++i)
        {
            result += j;
            result += container[i];
        }
        return result;
    }

    void replace_all(std::string& data,
                     const std::string& search,
                     const std::string& replace);

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
            {}

            inline sizer(const char s)
                : size(1)
            {}

            template <class T>
            inline sizer(T& s)
                : size(s.size())
            {}

            std::size_t size;
        };
    }

    template<typename... Args>
    inline std::string concat(const Args&... args)
    {
        size_t len = 0;
        for (auto s : std::initializer_list<concat_impl::sizer>{args...})  len += s.size;

        std::string result;
        result.reserve(len);
        concat_impl::concat_foreach(result, args...);
        return result;
    }

    // get the value corresponding to a key in a JSON object and assign it to target
    // if the key is not found, assign default_value to target
    template <typename T>
    void assign_or(nlohmann::json j, const char* key, T& target, T default_value)
    {
        if (j.contains(key))
            target = j[key];
        else
            target = default_value;
    }
}

#endif // MAMBA_UTIL_HPP
