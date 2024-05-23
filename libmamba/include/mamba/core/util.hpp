// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UTIL_HPP
#define MAMBA_CORE_UTIL_HPP

#include <chrono>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "mamba/core/error_handling.hpp"
#include "mamba/fs/filesystem.hpp"

#include "tl/expected.hpp"

#define MAMBA_EMPTY_SHA "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

namespace mamba
{
    class Context;

    // Used when we want a callback which does nothing.
    struct no_op
    {
        void operator()() const noexcept
        {
        }
    };

    bool lexists(const fs::u8path& p);
    bool lexists(const fs::u8path& p, std::error_code& ec);
    std::vector<fs::u8path> filter_dir(const fs::u8path& dir, const std::string& suffix);
    bool paths_equal(const fs::u8path& lhs, const fs::u8path& rhs);

    std::string
    read_contents(const fs::u8path& path, std::ios::openmode mode = std::ios::in | std::ios::binary);
    std::vector<std::string> read_lines(const fs::u8path& path);

    inline void make_executable(const fs::u8path& p)
    {
        fs::permissions(
            p,
            fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec
        );
    }

    // @return `true` if `TemporaryFile` will not delete files once destroy.
    //         If `set_persist_temporary_files` was not called, returns `false` by default.
    //
    // @warning This function must be called in the execution scope `main()`, doing otherwise leads
    // to undefined behavior.
    //
    // @warning This is a thread-safe accessor for a global parameter: the returned value is
    // therefore obsolete before being obtained and should be considered as a hint.
    bool must_persist_temporary_files();

    // Controls if `TemporaryFile` will delete files once destroy or not.
    // This is useful for debugging situations where temporary data lead to unexpected behavior.
    //
    // @warning This function must be called in the execution scope `main()`, doing otherwise leads
    // to undefined behavior.
    //
    // @warning This is a thread-safe function setting a global parameter: if concurrent threads
    // are both calling this function with different value there is no guarantee as to which
    // value will be retained.
    // However if there is exactly one thread executing this function then the following is true:
    //    const auto result = set_persist_temporary_files(must_persist);
    //    result == must_persist && must_persist_temporary_files() == must_persist
    bool set_persist_temporary_files(bool will_persist);

    // @return `true` if `TemporaryDirectory` will not delete files once destroy.
    //         If `set_persist_temporary_files` was not called, returns `false` by default.
    //
    // @warning This function must be called in the execution scope `main()`, doing otherwise leads
    // to undefined behavior.
    //
    // @warning This is a thread-safe accessor for a global parameter: the returned value is
    // therefore obsolete before being obtained and should be considered as a hint.
    bool must_persist_temporary_directories();

    // Controls if `TemporaryDirectory` will delete files once destroy or not.
    // This is useful for debugging situations where temporary data lead to unexpected behavior.
    //
    // @warning This function must be called in the execution scope `main()`, doing otherwise leads
    // to undefined behavior.
    //
    // @warning This is a thread-safe function setting a global parameter: if concurrent threads
    // are both calling this function with different value there is no guarantee as to which
    // value will be retained.
    // However if there is exactly one thread executing this function then the following is true:
    //    const auto result = set_persist_temporary_directories(must_persist);
    //    result == must_persist && must_persist_temporary_directories() == must_persist
    bool set_persist_temporary_directories(bool will_persist);

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

        TemporaryFile(
            const std::string& prefix = "mambaf",
            const std::string& suffix = "",
            const std::optional<fs::u8path>& dir = std::nullopt
        );
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

    // @return `true` if constructing a `LockFile` will result in locking behavior, `false` if
    // using `LockFile will not lock the file and behave like a no-op.
    //
    // @warning This function must be called in the execution scope `main()`, doing otherwise leads
    // to undefined behavior.
    //
    // @warning This is a thread-safe accessor for a global parameter: the returned value is
    // therefore obsolete before being obtained and should be considered as a hint.
    bool is_file_locking_allowed();


    // Controls if, with `true`, constructing a `LockFile` will result in locking behavior,
    // or, with `false, will not lock the file and behave like a no-op.
    //
    // @warning This function must be called in the execution scope `main()`, doing otherwise leads
    // to undefined behavior.
    //
    // @warning This is a thread-safe function setting a global parameter: if concurrent threads
    // are both calling this function with different value there is no guarantee as to which
    // value will be retained.
    // However if there is exactly one thread executing this function then the following is true:
    //    const auto result = allow_file_locking(allow);
    //    result == allow && is_file_locking_allowed() == allow
    bool allow_file_locking(bool allow);

    // @return The file locking timeout used by `LockFile` at construction.
    //
    // @warning This function must be called in the execution scope `main()`, doing otherwise leads
    // to undefined behavior.
    //
    // @warning This is a thread-safe accessor for a global parameter: the returned value is
    // therefore obsolete before being obtained and should be considered as a hint.
    std::chrono::seconds default_file_locking_timeout();

    // Changes the locking duration when `LockFile` is constructed without a specified locking
    // timeout.
    //
    // @warning This function must be called in the execution scope `main()`, doing otherwise leads
    // to undefined behavior.
    //
    // @warning This is a thread-safe function setting a global parameter: if concurrent threads
    // are both calling this function with different value there is no guarantee as to which
    // value will be retained.
    // However if there is exactly one thread executing this function then the following is true:
    //    const auto result = set_file_locking_timeout(timeout);
    //    result == timeout && default_file_locking_timeout() == timeout
    std::chrono::seconds set_file_locking_timeout(const std::chrono::seconds& new_timeout);

    // This is a non-throwing file-locking mechanism.
    // It can be used on a file or directory path. In the case of a directory path a file will be
    // created to be locked. The locking will be implemented using the OS's filesystem locking
    // capabilities, if available.
    //
    // Once constructed, use `is_locked()` or `operator bool` to check if the lock did happen
    // successfully. When locking fails because of an error, the error can be retrieved using
    // `error()`. When attempting to lock a path which is already locked by another process, the
    // attempt will fail and `is_locked()` will return false.
    //
    // When the same process attempts to lock the same path more than once (multiple instances of
    // `LockFile` target the same path), creating a new `LockFile` for that path will always succeed
    // and increment the lock owner count which can be retrieved using `count_lock_owners()`.
    // Basically, all instacnes of `LockFile` locking the same path are sharing the lock, which will
    // only be released once there is no instance alive.
    //
    // Use `mamba::allow_file_locking(false)` to never have locking happen, in which case
    // the created `LockFile` instance will not be locked (`is_locked()` will return false) but will
    // have no error either (`error()` will return `noopt`).
    //
    // Example:
    //      using namespace mamba;
    //      LockFile some_work_on(some_path)
    //      {
    //          LockFile lock{ some_path, timeout };
    //          if(lock) // make sure the locking happened
    //          {
    //              print("locked file {}, locking counts: {}", some_path,
    //              lock.count_lock_owners()); // success might mean we are locking the same path
    //              from multiple threads do_something(some_path); // locking was a success
    //          }
    //          else // locking didnt succeed for some reason
    //          {
    //              if(auto error = lock.error) print(error); // some error happened while
    //              attempting the lock, maybe some other process already locks the path else
    //              print("didn't attempt locking {}", some_path); // locking didn't happen for some
    //              other reason, maybe a configuration option
    //          }
    //          some_more_work(some_path); // do this that the lock failed or not
    //          return lock; // The locking ownership can be transferred to another function if
    //          necessary
    //      }
    //
    class LockFile
    {
    public:

        // Non-throwing constructors, attempting lock on the provided path, file or directory.
        // In case of a directory, a lock-file will be created, located at `this->lockfile_path()`
        // and `this->is_locked()` (and `if(*this))` will always return true (unless this instance
        // is moved-from). If the lock acquisition failed or `allow_file_locking(false)` and until
        // re-assigned:
        // - `this->is_locked() == false` and `if(*this) ...` will go in the `false` branch.
        // - accessors will throw, except `is_locked()`, `count_lock_owners()`, and `error()`
        explicit LockFile(const fs::u8path& path);
        LockFile(const fs::u8path& path, const std::chrono::seconds& timeout);

        ~LockFile();

        LockFile(const LockFile&) = delete;
        LockFile& operator=(const LockFile&) = delete;

        LockFile(LockFile&&);
        LockFile& operator=(LockFile&&);

        // Returns true if this LockFile is currently maintaining a lock on the target path.
        // Returns false if this instance have been moved-from without being re-assigned,
        // or if the lock acquisition failed.
        bool is_locked() const
        {
            return impl.has_value()                   // we have a owner
                   && (impl.value() ? true : false);  // it's not null
        }

        // Convenient operator to check if a lockfile is actually locking a path.
        explicit operator bool() const
        {
            return is_locked();
        }

        // Returns the fd of the path being locked, throws if `is_locked() == false`.
        int fd() const;

        // Returns the path being locked, throws if `is_locked() == false`.
        fs::u8path path() const;

        // Returns the path of the lock-file being locked, throws if `is_locked() == false`.
        fs::u8path lockfile_path() const;

        // Returns the count of LockFile instances which are currently locking
        // the same path/file from the same process.
        // Returns 0 if `is_locked() == false`.
        std::size_t count_lock_owners() const
        {
            return std::size_t(impl.has_value() ? impl.value().use_count() : 0);
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
            return lockfile.is_locked() &&
#ifdef _WIN32
                   is_locked(lockfile.lockfile_path());
#else
                   // Opening a new file descriptor on Unix would clear locks
                   is_locked(lockfile.fd());
#endif
        }

        std::optional<mamba_error> error() const
        {
            if (impl.has_value())
            {
                return {};
            }
            else
            {
                return impl.error();
            }
        }

    private:

        tl::expected<std::shared_ptr<LockFileOwner>, mamba_error> impl;
    };

    void split_package_extension(const std::string& file, std::string& name, std::string& extension);

    std::string
    quote_for_shell(const std::vector<std::string>& arguments, const std::string& shell = "");

    std::size_t clean_trash_files(const fs::u8path& prefix, bool deep_clean);
    std::size_t remove_or_rename(const Context& context, const fs::u8path& path);

    // Unindent a string literal
    std::string unindent(const char* p);

    std::string prepend(const std::string& p, const char* start, const char* newline = "");

    std::string prepend(const char* p, const char* start, const char* newline = "");

    std::string timestamp(const std::time_t& time);

    std::time_t utc_time_now();

    std::string utc_timestamp_now();

    std::time_t parse_utc_timestamp(const std::string& timestamp, int& error_code) noexcept;

    std::time_t parse_utc_timestamp(const std::string& timestamp);

    std::ofstream
    open_ofstream(const fs::u8path& path, std::ios::openmode mode = std::ios::out | std::ios::binary);

    std::ifstream
    open_ifstream(const fs::u8path& path, std::ios::openmode mode = std::ios::in | std::ios::binary);

    bool ensure_comspec_set();

    struct WrappedCallOptions
    {
        bool is_mamba_exe = false;
        bool dev_mode = false;
        bool debug_wrapper_scripts = false;

        static WrappedCallOptions from_context(const Context&);
    };

    std::unique_ptr<TemporaryFile> wrap_call(
        const Context& context,
        const fs::u8path& root_prefix,
        const fs::u8path& prefix,
        const std::vector<std::string>& arguments,  // TODO: c++20 replace by std::span
        WrappedCallOptions options = {}
    );

    struct PreparedWrappedCall
    {
        std::vector<std::string> wrapped_command;
        std::unique_ptr<TemporaryFile> temporary_file;
    };

    PreparedWrappedCall prepare_wrapped_call(
        const Context& context,
        const fs::u8path& prefix,
        const std::vector<std::string>& cmd
    );

    /// Returns `true` if the filename matches names of files which should be interpreted as YAML.
    /// NOTE: this does not check if the file exists.
    bool is_yaml_file_name(std::string_view filename);

    std::optional<std::string>
    proxy_match(const std::string& url, const std::map<std::string, std::string>& proxy_servers);

    std::string hide_secrets(std::string_view str);

    class non_copyable_base
    {
    public:

        non_copyable_base()
        {
        }

    private:

        non_copyable_base(const non_copyable_base&);
        non_copyable_base& operator=(const non_copyable_base&);
    };
}  // namespace mamba

#endif  // MAMBA_UTIL_HPP
