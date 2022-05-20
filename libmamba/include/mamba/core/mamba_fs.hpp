#ifndef MAMBA_CORE_FS_HPP
#define MAMBA_CORE_FS_HPP

#include <filesystem>
#include <fstream>
#include <string>

namespace fs
{
    // Allow all the filesystem algorithms and rely on implicit conversions from/to u8string
    using namespace std::filesystem;

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

        u8path(const std::filesystem::directory_entry& entry)
            : m_path(entry)
        {
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

        template <typename OutStream>
        friend OutStream& operator<<(OutStream& out, const u8path& path)
        {
            out << path.m_path;
            return out;
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

}


#endif
