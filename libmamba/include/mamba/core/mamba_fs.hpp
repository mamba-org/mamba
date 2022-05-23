#ifndef MAMBA_CORE_FS_HPP
#define MAMBA_CORE_FS_HPP

#include <filesystem>
#include <fstream>
#include <string>

namespace fs
{

    // Returns an utf-8 string given a standard path.
    inline std::string to_utf8(const std::filesystem::path& path)
    {
#if __cplusplus == 201703L
        return path.u8string();
#else
#error This function implementation is specific to C++17, using another version requires a different implementation here.
#endif
    }

    // Returns standard path given an utf-8 string.
    inline std::filesystem::path from_utf8(std::string_view u8string)
    {
#if __cplusplus == 201703L
        return std::filesystem::u8path(u8string);
#else
#error This function implementation is specific to C++17, using another version requires a different implementation here.
#endif
    }

    // Same as std::filesystem::path except we only accept and output UTF-8 paths
    class u8path
    {
    public:
        u8path() = default;

        // Copy is allowed.
        u8path(const u8path& other) = default;
        u8path& operator=(const u8path& other) = default;

        // Move is allowed.
        u8path(u8path&& other) = default;
        u8path& operator=(u8path&& other) = default;

        //---- Construction ----

        u8path(const std::filesystem::path& path)
            : m_path(path)
        {
        }

        u8path& operator=(const std::filesystem::path& path)
        {
            m_path = path;
            return *this;
        }

        u8path& operator=(std::filesystem::path&& path)
        {
            m_path = std::move(path);
            return *this;
        }

        u8path(const std::filesystem::directory_entry& entry)
            : m_path(entry)
        {
        }

        u8path(std::string_view u8string)
            : m_path(from_utf8(u8string))
        {
        }

        u8path& operator=(std::string_view u8string)
        {
            m_path = from_utf8(u8string);
            return *this;
        }

        u8path(const char* u8string)
            : m_path(from_utf8(u8string))
        {
        }

        u8path& operator=(const char* u8string)
        {
            m_path = from_utf8(u8string);
            return *this;
        }

        u8path(const std::string& u8string)
            : m_path(from_utf8(u8string))
        {
        }

        u8path& operator=(const std::string& u8string)
        {
            m_path = from_utf8(u8string);
            return *this;
        }

        u8path& operator=(std::string&& u8string)
        {
            m_path = from_utf8(std::move(u8string));
            return *this;
        }

        //---- Wide character support (Windows usage mostly) - no Unicode conversions.

        u8path(const wchar_t* wstr)
            : m_path(wstr)
        {
        }

        u8path& operator=(const wchar_t* wstr)
        {
            m_path = wstr;
            return *this;
        }

        u8path(const std::wstring& wstr)
            : m_path(wstr)
        {
        }

        u8path& operator=(const std::wstring& wstr)
        {
            m_path = wstr;
            return *this;
        }

        u8path& operator=(std::wstring&& wstr)
        {
            m_path = std::move(wstr);
            return *this;
        }

        //---- Append ----

        u8path operator/(const u8path& p) const
        {
            return m_path / p.m_path;
        }

        template <typename T>
        u8path operator/(const T& p) const
        {
            return m_path / from_utf8(p);
        }

        u8path& operator+=(const u8path& to_append)
        {
            m_path += to_append.m_path;
            return *this;
        }

        // Append UTF-8 string or character
        template <typename T>
        u8path& operator+=(const T& to_append)
        {
            m_path = from_utf8(this->string() + to_append);
            return *this;
        }

        //---- Conversions ----

        // Returns an utf-8 string.
        std::string string() const
        {
            return to_utf8(m_path);
        }

        // Returns an utf-8 string.
        operator std::string() const
        {
            return this->string();
        }

        // Returns the native wstring (UTF-16 on windows).
        std::wstring wstring() const
        {
            return m_path.wstring();
        }

        // Implicitly convert to native wstring (UTF-16 on windows).
        operator std::wstring() const
        {
            return this->wstring();
        }

        // Implicit conversion to standard path.
        operator std::filesystem::path() const
        {
            return m_path;
        }

        // Explicit conversion to standard path.
        const std::filesystem::path& std_path() const noexcept
        {
            return m_path;
        }

        //---- Parts ----

        u8path stem() const
        {
            return m_path.stem();
        }

        u8path parent_path() const
        {
            return m_path.parent_path();
        }

        u8path root_name() const
        {
            return m_path.root_name();
        }

        u8path root_directory() const
        {
            return m_path.root_directory();
        }

        u8path root_path() const
        {
            return m_path.root_path();
        }

        u8path filename() const
        {
            return m_path.filename();
        }

        u8path extension() const
        {
            return m_path.extension();
        }

        //---- Modifiers ----

        void clear() noexcept
        {
            m_path.clear();
        }

        u8path& remove_filename()
        {
            m_path.remove_filename();
            return *this;
        }

        u8path& replace_filename(const u8path replacement)
        {
            m_path.replace_filename(replacement.m_path);
            return *this;
        }

        u8path& replace_extension(const u8path replacement)
        {
            m_path.replace_extension(replacement.m_path);
            return *this;
        }

        //---- Operators ----

        friend bool operator==(const u8path& left, const u8path& right) noexcept
        {
            return left.m_path == right.m_path;
        }

        friend bool operator!=(const u8path& left, const u8path& right) noexcept
        {
            return left.m_path != right.m_path;
        }

        friend bool operator<(const u8path& left, const u8path& right) noexcept
        {
            return left.m_path < right.m_path;
        }

        friend bool operator<=(const u8path& left, const u8path& right) noexcept
        {
            return left.m_path <= right.m_path;
        }

        friend bool operator>(const u8path& left, const u8path& right) noexcept
        {
            return left.m_path > right.m_path;
        }

        friend bool operator>=(const u8path& left, const u8path& right) noexcept
        {
            return left.m_path >= right.m_path;
        }

        friend bool operator==(const u8path& left, const std::filesystem::path& right) noexcept
        {
            return left.m_path == right;
        }

        friend bool operator!=(const u8path& left, const std::filesystem::path& right) noexcept
        {
            return left.m_path != right;
        }

        friend bool operator<(const u8path& left, const std::filesystem::path& right) noexcept
        {
            return left.m_path < right;
        }

        friend bool operator<=(const u8path& left, const std::filesystem::path& right) noexcept
        {
            return left.m_path <= right;
        }

        friend bool operator>(const u8path& left, const std::filesystem::path& right) noexcept
        {
            return left.m_path > right;
        }

        friend bool operator>=(const u8path& left, const std::filesystem::path& right) noexcept
        {
            return left.m_path >= right;
        }

        friend bool operator==(const u8path& left, const std::string& right) noexcept
        {
            return left.m_path == from_utf8(right);
        }

        friend bool operator!=(const u8path& left, const std::string& right) noexcept
        {
            return left.m_path != from_utf8(right);
        }

        friend bool operator==(const u8path& left, const char* right) noexcept
        {
            return left.m_path == from_utf8(right);
        }

        friend bool operator!=(const u8path& left, const char* right) noexcept
        {
            return left.m_path != from_utf8(right);
        }


        //---- State ----

        bool empty() const noexcept
        {
            return m_path.empty();
        }

        //---- Utility ----

        // Writing to stream always using UTF-8.
        // Note: this will not work well on Windows with std::cout which doesnt know it's UTF-8
        //       In that case use `u8path::std_path()` instead.
        template <typename OutStream>
        friend OutStream& operator<<(OutStream& out, const u8path& path)
        {
            out << path.string();
            return out;
        }

        // Reads stream assuming UTF-8 encoding.
        template <typename InputStream>
        friend InputStream& operator>>(InputStream& in, u8path& path)
        {
            std::string raw_input;
            in >> raw_input;
            path.m_path = from_utf8(raw_input);
            return in;
        }

        friend std::size_t hash_value(const u8path& p) noexcept
        {
            return hash_value(p.m_path);
        }

        friend void swap(u8path& left, u8path& right) noexcept
        {
            swap(left.m_path, right.m_path);
        }

    private:
        std::filesystem::path m_path;
    };

    //---- Standard Filesystem element we reuse here -----

    using std::filesystem::perms;
    using std::filesystem::file_status;
    using std::filesystem::file_type;
    using std::filesystem::file_time_type;
    using std::filesystem::filesystem_error;
    using std::filesystem::perm_options;
    using std::filesystem::directory_iterator;
    using std::filesystem::recursive_directory_iterator;
    using std::filesystem::space_info;
    using std::filesystem::copy_options;
    using std::filesystem::directory_options;

    //----- Wrapped versions of std::filesystem algorithm that returns a `u8path` instead of `std::filesystem::path`

    //path absolute(const path& p);
    //path absolute(const path& p, error_code& ec);
    template <typename... OtherArgs>
    u8path absolute(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::absolute(path, std::forward<OtherArgs>(args)...);
    }

    //path canonical(const path& p);
    //path canonical(const path& p, error_code& ec);
    template <typename... OtherArgs>
    u8path canonical(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::canonical(path, std::forward<OtherArgs>(args)...);
    }

    //void copy(const path& from, const path& to);
    //void copy(const path& from, const path& to, error_code& ec);
    //void copy(const path& from, const path& to, copy_options options);
    //void copy(const path& from, const path& to, copy_options options, error_code& ec);
    template <typename... OtherArgs>
    void copy(const u8path& from, const u8path& to, OtherArgs&&... args)
    {
        std::filesystem::copy(from, to, std::forward<OtherArgs>(args)...);
    }

    //bool copy_file(const path& from, const path& to);
    //bool copy_file(const path& from, const path& to, error_code& ec);
    //bool copy_file(const path& from, const path& to, copy_options option);
    //bool copy_file(const path& from, const path& to, copy_options option, error_code& ec);
    template <typename... OtherArgs>
    bool copy_file(const u8path& from, const u8path& to, OtherArgs&&... args)
    {
        return std::filesystem::copy_file(from, to, std::forward<OtherArgs>(args)...);
    }

    //void copy_symlink(const path& existing_symlink, const path& new_symlink);
    //void copy_symlink(const path& existing_symlink,
    //                  const path& new_symlink,
    //                  error_code& ec) noexcept;
    template <typename... OtherArgs>
    void copy_symlink(const u8path& existing_symlink,
                      const u8path& new_symlink,
                      OtherArgs&&... args)
    {
        std::filesystem::copy_symlink(existing_symlink,
                                      new_symlink, std::forward<OtherArgs>(args)...);
    }

    //bool create_directories(const path& p);
    //bool create_directories(const path& p, error_code& ec);
    template <typename... OtherArgs>
    bool create_directories(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::create_directories(path, std::forward<OtherArgs>(args)...);
    }

    //bool create_directory(const path& p);
    //bool create_directory(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool create_directory(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::create_directory(path, std::forward<OtherArgs>(args)...);
    }

    //bool create_directory(const path& p, const path& attributes);
    //bool create_directory(const path& p, const path& attributes, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool create_directory(const u8path& path, const u8path& attributes, OtherArgs&&... args)
    {
        return std::filesystem::create_directory(path, attributes, std::forward<OtherArgs>(args)...);
    }


    //void create_directory_symlink(const path& to, const path& new_symlink);
    //void create_directory_symlink(const path& to, const path& new_symlink, error_code& ec) noexcept;
    template <typename... OtherArgs>
    void create_directory_symlink(const u8path& to,
                      const u8path& new_symlink,
                      OtherArgs&&... args)
    {
        std::filesystem::create_directory_symlink(to, new_symlink, std::forward<OtherArgs>(args)...);
    }

    //void create_hard_link(const path& to, const path& new_hard_link);
    //void create_hard_link(const path& to, const path& new_hard_link, error_code& ec) noexcept;
    template <typename... OtherArgs>
    void create_hard_link(const u8path& to, const u8path& new_hard_link, OtherArgs&&... args)
    {
        std::filesystem::create_hard_link(to, new_hard_link, std::forward<OtherArgs>(args)...);
    }

    //void create_symlink(const path& to, const path& new_symlink);
    //void create_symlink(const path& to, const path& new_symlink, error_code& ec) noexcept;
    template <typename... OtherArgs>
    void create_symlink(const u8path& to, const u8path& new_symlink, OtherArgs&&... args)
    {
        std::filesystem::create_symlink(to, new_symlink, std::forward<OtherArgs>(args)...);
    }

    //path current_path();
    inline u8path current_path()
    {
        return std::filesystem::current_path();
    }

    //path current_path(error_code& ec);
    inline u8path current_path(std::error_code& ec)
    {
        return std::filesystem::current_path(ec);
    }

    //void current_path(const path& p);
    //void current_path(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    void current_path(const u8path& path, OtherArgs&&... args)
    {
        std::filesystem::current_path(path, std::forward<OtherArgs>(args)...);
    }
    

    //bool equivalent(const path& p1, const path& p2);
    //bool equivalent(const path& p1, const path& p2, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool equivalent(const u8path& p1, const u8path& p2, OtherArgs&&... args)
    {
        return std::filesystem::equivalent(p1, p2, std::forward<OtherArgs>(args)...);
    }

    //bool exists(file_status s) noexcept;
    inline bool exists(file_status s) noexcept
    {
        return std::filesystem::exists(s);
    }

    //bool exists(const path& p);
    //bool exists(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool exists(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::exists(path, std::forward<OtherArgs>(args)...);
    }

    //uintmax_t file_size(const path& p);
    //uintmax_t file_size(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    uintmax_t file_size(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::file_size(path, std::forward<OtherArgs>(args)...);
    }

    //uintmax_t hard_link_count(const path& p);
    //uintmax_t hard_link_count(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    uintmax_t hard_link_count(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::hard_link_count(path, std::forward<OtherArgs>(args)...);
    }

    //bool is_block_file(file_status s) noexcept;
    inline bool is_block_file(file_status s) noexcept
    {
        return std::filesystem::is_block_file(s);
    }

    //bool is_block_file(const path& p);
    //bool is_block_file(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool is_block_file(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::is_block_file(path, std::forward<OtherArgs>(args)...);
    }

    //bool is_character_file(file_status s) noexcept;
    inline bool is_character_file(file_status s) noexcept
    {
        return std::filesystem::is_character_file(s);
    }

    //bool is_character_file(const path& p);
    //bool is_character_file(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool is_character_file(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::is_character_file(path, std::forward<OtherArgs>(args)...);
    }

    //bool is_directory(file_status s) noexcept;
    inline bool is_directory(file_status s) noexcept
    {
        return std::filesystem::is_directory(s);
    }

    //bool is_directory(const path& p);
    //bool is_directory(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool is_directory(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::is_directory(path, std::forward<OtherArgs>(args)...);
    }

    //bool is_empty(const path& p);
    //bool is_empty(const path& p, error_code& ec);
    template <typename... OtherArgs>
    bool is_empty(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::is_empty(path, std::forward<OtherArgs>(args)...);
    }

    //bool is_fifo(file_status s) noexcept;
    inline bool is_fifo(file_status s) noexcept
    {
        return std::filesystem::is_fifo(s);
    }

    //bool is_fifo(const path& p);
    //bool is_fifo(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool is_fifo(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::is_fifo(path, std::forward<OtherArgs>(args)...);
    }

    //bool is_other(file_status s) noexcept;
    inline bool is_other(file_status s) noexcept
    {
        return std::filesystem::is_other(s);
    }

    //bool is_other(const path& p);
    //bool is_other(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool is_other(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::is_other(path, std::forward<OtherArgs>(args)...);
    }

    //bool is_regular_file(file_status s) noexcept;
    inline bool is_regular_file(file_status s) noexcept
    {
        return std::filesystem::is_regular_file(s);
    }

    //bool is_regular_file(const path& p);
    //bool is_regular_file(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool is_regular_file(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::is_regular_file(path, std::forward<OtherArgs>(args)...);
    }

    //bool is_socket(file_status s) noexcept;
    inline bool is_socket(file_status s) noexcept
    {
        return std::filesystem::is_socket(s);
    }

    //bool is_socket(const path& p);
    //bool is_socket(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool is_socket(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::is_socket(path, std::forward<OtherArgs>(args)...);
    }

    //bool is_symlink(file_status s) noexcept;
    inline bool is_symlink(file_status s) noexcept
    {
        return std::filesystem::is_symlink(s);
    }

    //bool is_symlink(const path& p);
    //bool is_symlink(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool is_symlink(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::is_symlink(path, std::forward<OtherArgs>(args)...);
    }

    //file_time_type last_write_time(const path& p);
    //file_time_type last_write_time(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    file_time_type last_write_time(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::last_write_time(path, std::forward<OtherArgs>(args)...);
    }

    //void last_write_time(const path& p, file_time_type new_time);
    //void last_write_time(const path& p, file_time_type new_time, error_code& ec) noexcept;
    template <typename... OtherArgs>
    void last_write_time(const u8path& path, file_time_type new_time, OtherArgs&&... args)
    {
        return std::filesystem::last_write_time(path, new_time, std::forward<OtherArgs>(args)...);
    }

    //void permissions(const path& p, perms prms, perm_options opts = perm_options::replace);
    //void permissions(const path& p, perms prms, error_code& ec) noexcept;
    //void permissions(const path& p, perms prms, perm_options opts, error_code& ec);
    template <typename... OtherArgs>
    void permissions(const u8path& path, OtherArgs&&... args)
    {
        std::filesystem::permissions(path, std::forward<OtherArgs>(args)...);
    }

    //path proximate(const path& p, error_code& ec);
    //path proximate(const path& p, const path& base = current_path());
    template <typename... OtherArgs>
    u8path proximate(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::proximate(path, std::forward<OtherArgs>(args)...);
    }

    //path proximate(const path& p, const path& base, error_code& ec);
    template <typename... OtherArgs>
    u8path proximate(const u8path& path, const u8path& base, OtherArgs&&... args)
    {
        return std::filesystem::proximate(path, base, std::forward<OtherArgs>(args)...);
    }

    //path read_symlink(const path& p);
    //path read_symlink(const path& p, error_code& ec);
    template <typename... OtherArgs>
    u8path read_symlink(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::read_symlink(path, std::forward<OtherArgs>(args)...);
    }

    //path relative(const path& p, error_code& ec);
    //path relative(const path& p, const path& base = current_path());
    template <typename... OtherArgs>
    u8path relative(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::relative(path, std::forward<OtherArgs>(args)...);
    }

    //path relative(const path& p, const path& base, error_code& ec);
    template <typename... OtherArgs>
    u8path relative(const u8path& path, const u8path& base, OtherArgs&&... args)
    {
        return std::filesystem::relative(path, base, std::forward<OtherArgs>(args)...);
    }

    //bool remove(const path& p);
    //bool remove(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    bool remove(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::remove(path, std::forward<OtherArgs>(args)...);
    }

    //uintmax_t remove_all(const path& p);
    //uintmax_t remove_all(const path& p, error_code& ec);
    template <typename... OtherArgs>
    uintmax_t remove_all(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::remove_all(path, std::forward<OtherArgs>(args)...);
    }

    //void rename(const path& from, const path& to);
    //void rename(const path& from, const path& to, error_code& ec) noexcept;
    template <typename... OtherArgs>
    void rename(const u8path& from, const u8path& to, OtherArgs&&... args)
    {
        std::filesystem::rename(from, to, std::forward<OtherArgs>(args)...);
    }

    //void resize_file(const path& p, uintmax_t size);
    //void resize_file(const path& p, uintmax_t size, error_code& ec) noexcept;
    template <typename... OtherArgs>
    void resize_file(const u8path& path, OtherArgs&&... args)
    {
        std::filesystem::resize_file(path, std::forward<OtherArgs>(args)...);
    }

    //space_info space(const path& p);
    //space_info space(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    space_info space(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::space(path, std::forward<OtherArgs>(args)...);
    }

    //file_status status(const path& p);
    //file_status status(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    file_status status(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::status(path, std::forward<OtherArgs>(args)...);
    }

    //bool status_known(file_status s) noexcept;
    inline bool status_known(file_status s) noexcept
    {
        return std::filesystem::status_known(s);
    }

    //file_status symlink_status(const path& p);
    //file_status symlink_status(const path& p, error_code& ec) noexcept;
    template <typename... OtherArgs>
    file_status symlink_status(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::symlink_status(path, std::forward<OtherArgs>(args)...);
    }

    //path temp_directory_path();
    //path temp_directory_path(error_code& ec);
    template <typename... OtherArgs>
    u8path temp_directory_path(OtherArgs&&... args)
    {
        return std::filesystem::temp_directory_path(std::forward<OtherArgs>(args)...);
    }

    //path weakly_canonical(const path& p);
    //path weakly_canonical(const path& p, error_code& ec);
    template <typename... OtherArgs>
    u8path weakly_canonical(const u8path& path, OtherArgs&&... args)
    {
        return std::filesystem::weakly_canonical(path, std::forward<OtherArgs>(args)...);
    }

}


#endif
