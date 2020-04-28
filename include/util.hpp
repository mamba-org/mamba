#ifndef MAMBA_UTIL_HPP
#define MAMBA_UTIL_HPP

#include <stdexcept>
#include <string_view>
#include <string>


#include "thirdparty/filesystem.hpp"
#include "thirdparty/pystring14/pystring.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{
    #if __APPLE__ || __MACH__
    static constexpr bool on_win = false;
    static constexpr bool on_linux = false;
    static constexpr bool on_mac = true;
    #elif __linux__
    static constexpr bool on_win = false;
    static constexpr bool on_linux = false;
    static constexpr bool on_mac = true;
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

    std::string get_file_contents(const fs::path& path);

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

        TemporaryFile();
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

    inline std::string compute_short_python_version(const std::string& long_version)
    {
        auto sv = pystring::split(long_version, ".", 2);
        return concat(sv[0], '.', sv[1]);
    }

    // supply short python version, e.g. 2.7, 3.5...
    inline fs::path get_python_short_path(const std::string& python_version)
    {
        #ifdef _WIN32
        return "python.exe";
        #else
        return fs::path("bin") / concat("python", python_version);
        #endif
    }

    inline fs::path get_python_site_packages_short_path(const std::string& python_version)
    {
        if (python_version.size() == 0)
        {
            return fs::path();
        }

        #ifdef _WIN32
        return fs::path("Lib") / "site-packages";
        #else
        return fs::path("lib") / concat("python", python_version) / "site-packages";
        #endif
    }

    inline fs::path get_bin_directory_short_path()
    {
        #ifdef _WIN32
        return "Scripts";
        #else
        return "bin";
        #endif
    }

    inline fs::path get_python_noarch_target_path(const std::string& source_short_path, const fs::path& target_site_packages_short_path)
    {
        if (starts_with(source_short_path, "site-packages/"))
        {
            // replace `site_packages/` with prefix/site_packages
            return target_site_packages_short_path / source_short_path.substr(14, source_short_path.size() - 14);
        }
        else if (starts_with(source_short_path, "python-scripts/"))
        {
            return get_bin_directory_short_path() / source_short_path.substr(15, source_short_path.size() - 15);
        }
        else
        {
            return source_short_path;
        }
    }
}

#endif // MAMBA_UTIL_HPP
