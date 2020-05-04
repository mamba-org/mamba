#ifndef MAMBA_UTIL_HPP
#define MAMBA_UTIL_HPP

#include <stdexcept>
#include <string_view>
#include <string>


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

    bool starts_with(const std::string_view& str, const std::string_view& prefix);
    bool ends_with(const std::string_view& str, const std::string_view& suffix);

    std::string_view strip(const std::string_view& input);
    std::string_view lstrip(const std::string_view& input);
    std::string_view rstrip(const std::string_view& input);

    std::string_view strip(const std::string_view& input, const std::string_view& chars);
    std::string_view lstrip(const std::string_view& input, const std::string_view& chars);
    std::string_view rstrip(const std::string_view& input, const std::string_view& chars);

    std::vector<std::string_view> split(const std::string_view& input,
                                        char sep = ' ',
                                        std::size_t max_split = SIZE_MAX);

    std::vector<std::string_view> rsplit(const std::string_view& input,
                                         char sep = ' ',
                                         std::size_t max_split = SIZE_MAX);

    template <class S>
    inline std::string join(char j, const S& container)
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
}

#endif // MAMBA_UTIL_HPP
