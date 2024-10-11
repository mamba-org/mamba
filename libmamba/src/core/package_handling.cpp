// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <archive.h>
#include <archive_entry.h>
#include <reproc++/run.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/package_paths.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/util/string.hpp"
#include "mamba/validation/tools.hpp"

#include "../download/compression.hpp"
#include "nlohmann/json.hpp"

namespace mamba
{
    ExtractOptions ExtractOptions::from_context(const Context& context)
    {
        return {
            /* .sparse = */ context.extract_sparse,
            /* .subproc_mode = */ context.command_params.is_mamba_exe
                ? extract_subproc_mode::mamba_exe
                : extract_subproc_mode::mamba_package,
        };
    }

    class extraction_guard
    {
    public:

        explicit extraction_guard(const fs::u8path& file)
            : m_file(file)
        {
        }

        ~extraction_guard()
        {
            if (is_sig_interrupted())
            {
                LOG_INFO << "Extraction interrupted, erasing " << m_file.string();
                try
                {
                    fs::remove_all(m_file);
                }
                catch (std::exception& e)
                {
                    LOG_ERROR << "Removing failed, error: " << e.what();
                }
            }
        }

        extraction_guard(const extraction_guard&) = delete;
        extraction_guard& operator=(const extraction_guard&) = delete;
        extraction_guard(extraction_guard&&) = delete;
        extraction_guard& operator=(extraction_guard&&) = delete;

    private:

        const fs::u8path& m_file;
    };

    class scoped_archive_read : non_copyable_base
    {
    public:

        scoped_archive_read()
            : scoped_archive_read(archive_read_new()){};

        static scoped_archive_read read_disk()
        {
            return scoped_archive_read(archive_read_disk_new());
        }

        ~scoped_archive_read()
        {
            archive_read_free(m_archive);
        }

        operator archive*()
        {
            return m_archive;
        }

    private:

        explicit scoped_archive_read(archive* a)
            : m_archive(a)
        {
            if (!m_archive)
            {
                throw std::runtime_error("Could not create libarchive read object");
            }
        }

        archive* m_archive;
    };

    class scoped_archive_write : non_copyable_base
    {
    public:

        scoped_archive_write()
            : scoped_archive_write(archive_write_new())
        {
        }

        static scoped_archive_write write_disk()
        {
            return scoped_archive_write(archive_write_disk_new());
        }

        ~scoped_archive_write()
        {
            archive_write_free(m_archive);
        }

        operator archive*()
        {
            return m_archive;
        }

    private:

        explicit scoped_archive_write(archive* a)
            : m_archive(a)
        {
            if (!m_archive)
            {
                throw std::runtime_error("Could not create libarchive write object");
            }
        }

        archive* m_archive;
    };

    class scoped_archive_entry : non_copyable_base
    {
    public:

        scoped_archive_entry()
            : m_entry(archive_entry_new())
        {
            if (!m_entry)
            {
                throw std::runtime_error("Could not create libarchive entry object");
            }
        }

        ~scoped_archive_entry()
        {
            archive_entry_free(m_entry);
        }

        operator archive_entry*()
        {
            return m_entry;
        }

    private:

        archive_entry* m_entry;
    };

    void stream_extract_archive(
        scoped_archive_read& a,
        const fs::u8path& destination,
        const ExtractOptions& options
    );

    static int copy_data(scoped_archive_read& ar, scoped_archive_write& aw)
    {
        int r = 0;
        const void* buff = nullptr;
        std::size_t size = 0;
        la_int64_t offset = 0;

        while (true && !is_sig_interrupted())
        {
            r = archive_read_data_block(ar, &buff, &size, &offset);
            if (r == ARCHIVE_EOF)
            {
                return ARCHIVE_OK;
            }
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(ar));
            }
            r = static_cast<int>(archive_write_data_block(aw, buff, size, offset));
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(aw));
            }
        }
        return r;
    }

    bool path_has_prefix(const fs::u8path& path, const fs::u8path& prefix)
    {
        auto pair = std::mismatch(
            path.std_path().begin(),
            path.std_path().end(),
            prefix.std_path().begin(),
            prefix.std_path().end()
        );
        return pair.second == prefix.std_path().end();
    }

    int order(const fs::u8path& path)
    {
        int is_info = path_has_prefix(path, "info");
        return !is_info;
    }

    int zip_order(const fs::u8path& path)
    {
        // sort info-...tar.zst file last in zip folder"
        int init_order = util::starts_with(path.filename().string(), "info-");
        // sort metadata.json first in zip folder
        if (path.filename().string() == "metadata.json")
        {
            init_order = -1;
        }
        return init_order;
    }

    // Bundle up all files in directory and create destination archive
    void create_archive(
        const fs::u8path& directory,
        const fs::u8path& destination,
        compression_algorithm ca,
        int compression_level,
        int compression_threads,
        bool (*filter)(const fs::u8path&)
    )
    {
        int r;

        extraction_guard g(destination);

        fs::u8path abs_out_path = fs::absolute(destination);
        scoped_archive_write a;
        if (ca == compression_algorithm::bzip2)
        {
            archive_write_set_format_gnutar(a);
            archive_write_set_format_pax_restricted(a);  // Note 1
            archive_write_add_filter_bzip2(a);

            if (compression_level < 0 || compression_level > 9)
            {
                throw std::runtime_error("bzip2 compression level should be between 0 and 9");
            }
            std::string comp_level = std::string("bzip2:compression-level=")
                                     + std::to_string(compression_level);
            archive_write_set_options(a, comp_level.c_str());
        }
        if (ca == compression_algorithm::zip)
        {
            archive_write_set_format_zip(a);

            if (compression_level < 0 || compression_level > 9)
            {
                throw std::runtime_error("zip compression level should be between 0 and 9");
            }
            std::string comp_level = std::string("zip:compression-level=")
                                     + std::to_string(compression_level);
            archive_write_set_options(a, comp_level.c_str());
        }
        if (ca == compression_algorithm::zstd)
        {
            archive_write_set_format_gnutar(a);
            archive_write_set_format_pax_restricted(a);  // Note 1
            archive_write_add_filter_zstd(a);

            if (compression_level < 1 || compression_level > 22)
            {
                throw std::runtime_error("zstd compression level should be between 1 and 22");
            }

            std::string comp_level = std::string("zstd:compression-level=")
                                     + std::to_string(compression_level);

            int res = archive_write_set_options(a, comp_level.c_str());
            if (res != 0)
            {
                LOG_ERROR << "libarchive error (" << res << ") " << archive_error_string(a);
            }

            if (compression_threads > 2)
            {
                std::string comp_threads_level = std::string("zstd:threads=")
                                                 + std::to_string(compression_threads);
                res = archive_write_set_options(a, comp_threads_level.c_str());
                if (res != 0)
                {
                    LOG_ERROR << "libarchive error (" << res << ") " << archive_error_string(a);
                }
            }
        }

        archive_write_open_filename(a, abs_out_path.string().c_str());

        auto prev_path = fs::current_path();
        if (!fs::exists(directory))
        {
            throw std::runtime_error("Directory does not exist.");
        }
        fs::current_path(directory);

        std::vector<std::pair<int, fs::u8path>> files;
        if (ca != compression_algorithm::zip)
        {
            for (auto& dir_entry : fs::recursive_directory_iterator("."))
            {
                auto clean_path = dir_entry.path().lexically_relative("./");
                files.push_back({ order(clean_path), clean_path });
            }
        }
        else
        {
            // for zip files, sort `info` last
            for (auto& dir_entry : fs::directory_iterator("."))
            {
                auto clean_path = dir_entry.path().lexically_relative("./");
                files.push_back({ zip_order(clean_path), clean_path });
            }
        }

        std::sort(files.begin(), files.end());

        for (auto& order_pair : files)
        {
            const fs::u8path& path = order_pair.second;

            // skip adding _empty_ directories (they are implicitly added by the files therein)
            auto status = fs::symlink_status(path);
            if (fs::is_directory(status) && !fs::is_empty(path) && !fs::is_symlink(status))
            {
                LOG_INFO << "Skipping " << path << " as it is a non-empty directory.";
                continue;
            }

            LOG_INFO << "Adding " << path << " to archive";

            std::string p = path.string();
            if (filter && filter(p))
            {
                continue;
            }

            scoped_archive_entry entry;
            scoped_archive_read disk = scoped_archive_read::read_disk();
            if (archive_read_disk_set_behavior(disk, 0) < ARCHIVE_OK)
            {
                throw std::runtime_error(
                    util::concat("libarchive error: ", archive_error_string(disk))
                );
            }
            if (archive_read_disk_open(disk, p.c_str()) < ARCHIVE_OK)
            {
                throw std::runtime_error(
                    util::concat("libarchive error: ", archive_error_string(disk))
                );
            }
            if (archive_read_next_header2(disk, entry) < ARCHIVE_OK)
            {
                throw std::runtime_error(
                    util::concat("libarchive error: ", archive_error_string(disk))
                );
            }

            // clean out UID and GID
            archive_entry_set_uid(entry, 0);
            archive_entry_set_gid(entry, 0);
            archive_entry_set_gname(entry, "");
            archive_entry_set_uname(entry, "");

            if (archive_read_disk_descend(disk) < ARCHIVE_OK)
            {
                throw std::runtime_error(
                    util::concat("libarchive error: ", archive_error_string(disk))
                );
            }
            if (archive_write_header(a, entry) < ARCHIVE_OK)
            {
                throw std::runtime_error(util::concat("libarchive error: ", archive_error_string(a)));
            }


            if (!fs::is_symlink(p))
            {
                std::array<char, 8192> buffer;
                std::ifstream fin(p, std::ios::in | std::ios::binary);
                while (!fin.eof() && !is_sig_interrupted())
                {
                    fin.read(buffer.data(), buffer.size());
                    std::streamsize len = fin.gcount();
                    archive_write_data(a, buffer.data(), static_cast<std::size_t>(len));
                }
            }

            r = archive_write_finish_entry(a);
            if (r == ARCHIVE_WARN)
            {
                LOG_WARNING << "libarchive warning: " << archive_error_string(a);
            }
            else if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(util::concat("libarchive error: ", archive_error_string(a)));
            }
        }

        fs::current_path(prev_path);
    }

    // note the info folder must have already been created!
    void create_package(
        const fs::u8path& directory,
        const fs::u8path& out_file,
        int compression_level,
        int compression_threads
    )
    {
        fs::u8path out_file_abs = fs::absolute(out_file);
        if (util::ends_with(out_file.string(), ".tar.bz2"))
        {
            create_archive(
                directory,
                out_file_abs,
                bzip2,
                compression_level,
                compression_threads,
                [](const fs::u8path&) { return false; }
            );
        }
        else if (util::ends_with(out_file.string(), ".conda"))
        {
            TemporaryDirectory tdir;
            create_archive(
                directory,
                tdir.path() / util::concat("info-", out_file.stem().string(), ".tar.zst"),
                zstd,
                compression_level,
                compression_threads,
                [](const fs::u8path& p) -> bool {
                    return p.std_path().begin() != p.std_path().end()
                           && *p.std_path().begin() != "info";
                }
            );
            create_archive(
                directory,
                tdir.path() / util::concat("pkg-", out_file.stem().string(), ".tar.zst"),
                zstd,
                compression_level,
                compression_threads,
                [](const fs::u8path& p) -> bool {
                    return p.std_path().begin() != p.std_path().end()
                           && *p.std_path().begin() == "info";
                }
            );

            nlohmann::json pkg_metadata;
            pkg_metadata["conda_pkg_format_version"] = 2;
            const auto metadata_file_path = tdir.path() / "metadata.json";
            std::ofstream metadata_file(metadata_file_path.std_path());
            metadata_file << pkg_metadata;
            metadata_file.close();

            create_archive(
                tdir.path(),
                out_file_abs,
                zip,
                0,
                compression_threads,
                [](const fs::u8path&) { return false; }
            );
        }
    }

    void
    extract_archive(const fs::u8path& file, const fs::u8path& destination, const ExtractOptions& options)
    {
        LOG_INFO << "Extracting " << file << " to " << destination;
        extraction_guard g(destination);

        scoped_archive_read a;
        archive_read_support_format_tar(a);
        archive_read_support_format_zip(a);
        archive_read_support_filter_all(a);

        auto lock = LockFile(file);
        int r = archive_read_open_filename(a, file.string().c_str(), 10240);

        if (r != ARCHIVE_OK)
        {
            LOG_ERROR << "Error opening archive: " << archive_error_string(a);
            throw std::runtime_error(file.string() + " : Could not open archive for reading.");
        }

        stream_extract_archive(a, destination, options);
    }

    namespace
    {
        struct conda_extract_context : non_copyable_base
        {
            conda_extract_context(scoped_archive_read& lsource)
                : source(lsource)
                , buffer(download::get_zstd_buff_out_size())
            {
            }

            archive* source;
            std::vector<char> buffer;
        };
    }

    void stream_extract_archive(
        scoped_archive_read& a,
        const fs::u8path& destination,
        const ExtractOptions& options
    )
    {
        auto prev_path = fs::current_path();
        if (!fs::exists(destination))
        {
            fs::create_directories(destination);
        }
        fs::current_path(destination);

        /* Select which attributes we want to restore. */
        int flags = ARCHIVE_EXTRACT_TIME;
        flags |= ARCHIVE_EXTRACT_PERM;
        flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
        flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
        flags |= ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;
        flags |= ARCHIVE_EXTRACT_UNLINK;

        if (options.sparse)
        {
            flags |= ARCHIVE_EXTRACT_SPARSE;
        }

        scoped_archive_write ext = scoped_archive_write::write_disk();
        archive_write_disk_set_options(ext, flags);
        archive_write_disk_set_standard_lookup(ext);

        int r;
        archive_entry* entry;
        for (;;)
        {
            if (is_sig_interrupted())
            {
                throw std::runtime_error("SIGINT received. Aborting extraction.");
            }

            r = archive_read_next_header(a, &entry);
            if (r == ARCHIVE_EOF)
            {
                break;
            }
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(a));
            }

            r = archive_write_header(ext, entry);
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(ext));
            }
            else if (archive_entry_size(entry) > 0)
            {
                r = copy_data(a, ext);
                if (r < ARCHIVE_OK)
                {
                    const char* err_str = archive_error_string(ext);
                    if (err_str == nullptr)
                    {
                        err_str = archive_error_string(a);
                    }
                    if (err_str != nullptr)
                    {
                        throw std::runtime_error(err_str);
                    }
                    throw std::runtime_error("Extraction: writing data was not successful.");
                }
            }
            r = archive_write_finish_entry(ext);
            if (r == ARCHIVE_WARN)
            {
                LOG_WARNING << "libarchive warning: " << archive_error_string(a);
            }
            else if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(ext));
            }
        }

        fs::current_path(prev_path);
    }

    static la_ssize_t file_read(archive*, void* client_data, const void** buff)
    {
        conda_extract_context* mine = static_cast<conda_extract_context*>(client_data);
        *buff = mine->buffer.data();

        auto read = archive_read_data(mine->source, mine->buffer.data(), mine->buffer.size());
        if (read < 0)
        {
            throw std::runtime_error(
                fmt::format("Error reading from archive: {}", archive_error_string(mine->source))
            );
        }
        return read;
    }

    int archive_read_open_archive_entry(scoped_archive_read& a, conda_extract_context* ctx)
    {
        archive_clear_error(a);
        archive_read_set_read_callback(a, file_read);
        archive_read_set_callback_data(a, ctx);
        return archive_read_open1(a);
    }

    void extract_conda(
        const fs::u8path& file,
        const fs::u8path& dest_dir,
        const ExtractOptions& options,
        const std::vector<std::string>& parts
    )
    {
        scoped_archive_read a;
        archive_read_support_format_zip(a);

        conda_extract_context extract_context(a);

        if (archive_read_open_filename(a, file.string().c_str(), extract_context.buffer.size())
            != ARCHIVE_OK)
        {
            throw std::runtime_error(archive_error_string(a));
        }

        auto check_parts = [&parts](const std::string& name)
        {
            std::size_t pos = name.find_first_of('-');
            if (pos == std::string::npos)
            {
                return false;
            }
            std::string part = name.substr(0, pos);
            if (std::find(parts.begin(), parts.end(), part) != parts.end())
            {
                return true;
            }
            return false;
        };

        int r;
        archive_entry* entry;
        for (;;)
        {
            if (is_sig_interrupted())
            {
                throw std::runtime_error("SIGINT received. Aborting extraction.");
            }

            r = archive_read_next_header(a, &entry);
            if (r == ARCHIVE_EOF)
            {
                break;
            }
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(a));
            }

            fs::u8path p(archive_entry_pathname(entry));
            if (p.extension() == ".zst" && check_parts(p.filename().string()))
            {
                // extract zstd file
                scoped_archive_read inner;
                archive_read_support_filter_zstd(inner);
                archive_read_support_format_tar(inner);

                archive_read_open_archive_entry(inner, &extract_context);
                stream_extract_archive(inner, dest_dir, options);
            }
            else if (p.filename() == "metadata.json")
            {
                std::size_t json_size = static_cast<std::size_t>(archive_entry_size(entry));
                if (json_size == 0)
                {
                    LOG_INFO << "Package contains empty metadata.json file (" << file << ")";
                    continue;
                }
                std::string json(json_size, '\0');
                archive_read_data(a, json.data(), json_size);
                try
                {
                    auto obj = nlohmann::json::parse(json);
                    if (obj["conda_pkg_format_version"] != 2)
                    {
                        LOG_WARNING << "Unsupported conda package format version (" << file
                                    << ") - still trying to extract";
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_WARNING << "Error parsing metadata.json (" << file << "): " << e.what();
                }
            }
        }
    }

    static fs::u8path extract_dest_dir(const fs::u8path& file)
    {
        if (util::ends_with(file.string(), ".tar.bz2"))
        {
            return file.string().substr(0, file.string().size() - 8);
        }
        else if (util::ends_with(file.string(), ".conda"))
        {
            return file.string().substr(0, file.string().size() - 6);
        }
        LOG_ERROR << "Unknown package format '" << file.string() << "'";
        throw std::runtime_error("Unknown package format.");
    }

    void extract(const fs::u8path& file, const fs::u8path& dest, const ExtractOptions& options)
    {
        static std::mutex extract_mutex;
        std::lock_guard<std::mutex> lock(extract_mutex);

        if (util::ends_with(file.string(), ".tar.bz2"))
        {
            extract_archive(file, dest, options);
        }
        else if (util::ends_with(file.string(), ".conda"))
        {
            extract_conda(file, dest, options);
        }
        else
        {
            LOG_ERROR << "Unknown package format '" << file.string() << "'";
            throw std::runtime_error("Unknown package format.");
        }
    }

    fs::u8path extract(const fs::u8path& file, const ExtractOptions& options)
    {
        const fs::u8path dest_dir = extract_dest_dir(file);
        extract(file, dest_dir, options);
        return dest_dir;
    }

    void
    extract_subproc(const fs::u8path& file, const fs::u8path& dest, const ExtractOptions& options)
    {
        std::vector<std::string> args;
        if (options.subproc_mode == extract_subproc_mode::mamba_exe)
        {
            args = { get_self_exe_path().string(), "package", "extract", file.string(), dest.string() };
        }
        else
        {
            args = { "mamba-package", "extract", file.string(), dest.string() };
        }

        std::string out, err;
        LOG_DEBUG << "Running subprocess extraction '" << util::join(" ", args) << "'";
        auto [status, ec] = reproc::run(
            args,
            reproc::options{},
            reproc::sink::string(out),
            reproc::sink::string(err)
        );

        if (ec)
        {
            LOG_DEBUG << "Subprocess extraction exited with code " << ec << ", stdout: " << out
                      << ", stderr: " << err;
            LOG_DEBUG << "Running in-process extraction for '" << file.string() << "'";
            extract(file, dest, options);
        }
    }

    bool transmute(
        const fs::u8path& pkg_file,
        const fs::u8path& target,
        int compression_level,
        int compression_threads,
        const ExtractOptions& options
    )
    {
        TemporaryDirectory extract_dir;

        if (util::ends_with(pkg_file.string(), ".tar.bz2"))
        {
            extract_archive(pkg_file, extract_dir, options);
        }
        else if (util::ends_with(pkg_file.string(), ".conda"))
        {
            extract_conda(pkg_file, extract_dir, options);
        }
        else
        {
            throw std::runtime_error("Unknown package format (" + pkg_file.string() + ")");
        }

        create_package(extract_dir, target, compression_level, compression_threads);
        return true;
    }

    bool validate(const fs::u8path& pkg_folder, const ValidationParams& params)
    {
        auto safety_checks = params.safety_checks;
        if (safety_checks == VerificationLevel::Disabled)
        {
            return true;
        }

        bool is_warn = safety_checks == VerificationLevel::Warn;
        bool is_fail = safety_checks == VerificationLevel::Enabled;
        bool full_validation = params.extra_safety_checks;

        try
        {
            auto paths_data = read_paths(pkg_folder);
            for (auto& p : paths_data)
            {
                fs::u8path full_path = pkg_folder / p.path;
                // "exists" follows symlink so if the symlink doesn't link to existing target it
                // will return false. There is such symlink in _openmp_mutex package. So if the file
                // is a symlink we don't want to follow the symlink. The "paths_data" should include
                // path of all the files and we should not need to follow symlink.
                std::error_code ec;
                auto exists = lexists(full_path, ec);
                if (ec)
                {
                    LOG_WARNING << "Could not check existence: " << ec.message() << " (" << p.path
                                << ")";
                }
                if (!exists)
                {
                    if (is_warn || is_fail)
                    {
                        LOG_WARNING << "Invalid package cache, file '" << full_path.string()
                                    << "' is missing";
                        return false;
                    }
                }

                // old packages don't have paths.json with validation information
                if (p.size_in_bytes != 0)
                {
                    bool is_invalid = false;
                    if (p.path_type != PathType::SOFTLINK
                        && !validation::file_size(full_path, p.size_in_bytes))
                    {
                        LOG_WARNING << "Invalid package cache, file '" << full_path.string()
                                    << "' has incorrect size";
                        is_invalid = true;
                        if (is_fail)
                        {
                            return false;
                        }
                    }
                    if (full_validation && !is_invalid && p.path_type != PathType::SOFTLINK
                        && !(validation::sha256sum(full_path) == p.sha256))
                    {
                        LOG_WARNING << "Invalid package cache, file '" << full_path.string()
                                    << "' has incorrect SHA-256 checksum";
                        if (is_fail)
                        {
                            return false;
                        }
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_WARNING << "Invalid package cache, could not read 'paths.json' from '"
                        << pkg_folder.string() << "': " << e.what() << std::endl;
            return false;
        }
        return true;
    }
}  // namespace mamba
