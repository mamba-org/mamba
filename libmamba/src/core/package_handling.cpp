// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <archive.h>
#include <archive_entry.h>

#include <sstream>

#include <reproc++/run.hpp>

#include "nlohmann/json.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/package_paths.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/validate.hpp"

namespace mamba
{
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

    static int copy_data(archive* ar, archive* aw)
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
            r = archive_write_data_block(aw, buff, size, offset);
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(aw));
            }
        }
        return r;
    }

    // Bundle up all files in directory and create destination archive
    void create_archive(const fs::u8path& directory,
                        const fs::u8path& destination,
                        compression_algorithm ca,
                        int compression_level,
                        bool (*filter)(const std::string&))
    {
        int r;
        struct archive* a;

        extraction_guard g(destination);

        fs::u8path abs_out_path = fs::absolute(destination);
        a = archive_write_new();
        if (ca == compression_algorithm::bzip2)
        {
            archive_write_set_format_gnutar(a);
            archive_write_set_format_pax_restricted(a);  // Note 1
            archive_write_add_filter_bzip2(a);

            if (compression_level < 0 || compression_level > 9)
                throw std::runtime_error("bzip2 compression level should be between 0 and 9");
            std::string comp_level
                = std::string("bzip2:compression-level=") + std::to_string(compression_level);
            archive_write_set_options(a, comp_level.c_str());
        }
        if (ca == compression_algorithm::zip)
        {
            archive_write_set_format_zip(a);

            if (compression_level < 0 || compression_level > 9)
                throw std::runtime_error("zip compression level should be between 0 and 9");
            std::string comp_level
                = std::string("zip:compression-level=") + std::to_string(compression_level);
            archive_write_set_options(a, comp_level.c_str());
        }
        if (ca == compression_algorithm::zstd)
        {
            archive_write_set_format_gnutar(a);
            archive_write_set_format_pax_restricted(a);  // Note 1
            archive_write_add_filter_zstd(a);

            if (compression_level < 1 || compression_level > 22)
                throw std::runtime_error("zip compression level should be between 1 and 22");
            std::string comp_level
                = std::string("zstd:compression-level=") + std::to_string(compression_level);
            archive_write_set_options(a, comp_level.c_str());
        }

        archive_write_open_filename(a, abs_out_path.string().c_str());

        auto prev_path = fs::current_path();
        if (!fs::exists(directory))
        {
            throw std::runtime_error("Directory does not exist.");
        }
        fs::current_path(directory);

        for (auto& dir_entry : fs::recursive_directory_iterator("."))
        {
            // we only add empty directories to the archive
            if (dir_entry.is_directory() && !fs::is_empty(dir_entry.path()))
            {
                continue;
            }

            std::string p = dir_entry.path().string();
            // do this in a better way?
            if (p[0] == '.')
            {
                p = p.substr(1);
            }
            if (p[0] == '/')
            {
                p = p.substr(1);
            }
            if (filter && filter(p))
            {
                continue;
            }

            struct archive_entry* entry;
            entry = archive_entry_new();

            struct archive* disk;

            disk = archive_read_disk_new();
            if (disk == nullptr)
            {
                throw std::runtime_error(concat("libarchive error: could not create read_disk"));
            }
            if (archive_read_disk_set_behavior(disk, 0) < ARCHIVE_OK)
            {
                throw std::runtime_error(concat("libarchive error: ", archive_error_string(disk)));
            }
            if (archive_read_disk_open(disk, p.c_str()) < ARCHIVE_OK)
            {
                throw std::runtime_error(concat("libarchive error: ", archive_error_string(disk)));
            }
            if (archive_read_next_header2(disk, entry) < ARCHIVE_OK)
            {
                throw std::runtime_error(concat("libarchive error: ", archive_error_string(disk)));
            }
            if (archive_read_disk_descend(disk) < ARCHIVE_OK)
            {
                throw std::runtime_error(concat("libarchive error: ", archive_error_string(disk)));
            }
            if (archive_write_header(a, entry) < ARCHIVE_OK)
            {
                throw std::runtime_error(concat("libarchive error: ", archive_error_string(a)));
            }


            if (!fs::is_symlink(p))
            {
                std::array<char, 8192> buffer;
                std::ifstream fin(p, std::ios::in | std::ios::binary);
                while (!fin.eof() && !is_sig_interrupted())
                {
                    fin.read(buffer.data(), buffer.size());
                    std::streamsize len = fin.gcount();
                    archive_write_data(a, buffer.data(), len);
                }
            }

            r = archive_write_finish_entry(a);
            if (r == ARCHIVE_WARN)
            {
                LOG_WARNING << "libarchive warning: " << archive_error_string(a);
            }
            else if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(concat("libarchive error: ", archive_error_string(a)));
            }

            archive_read_close(disk);
            archive_read_free(disk);
            archive_entry_clear(entry);
        }

        archive_write_close(a);  // Note 4
        archive_write_free(a);   // Note 5
        fs::current_path(prev_path);
    }

    // note the info folder must have already been created!
    void create_package(const fs::u8path& directory,
                        const fs::u8path& out_file,
                        int compression_level)
    {
        fs::u8path out_file_abs = fs::absolute(out_file);
        if (ends_with(out_file.string(), ".tar.bz2"))
        {
            create_archive(directory,
                           out_file_abs,
                           bzip2,
                           compression_level,
                           [](const std::string&) { return false; });
        }
        else if (ends_with(out_file.string(), ".conda"))
        {
            TemporaryDirectory tdir;
            create_archive(directory,
                           tdir.path() / concat("info-", out_file.stem().string(), ".tar.zst"),
                           zstd,
                           compression_level,
                           [](const std::string& p) -> bool { return !starts_with(p, "info/"); });
            create_archive(directory,
                           tdir.path() / concat("pkg-", out_file.stem().string(), ".tar.zst"),
                           zstd,
                           compression_level,
                           [](const std::string& p) -> bool { return starts_with(p, "info/"); });

            nlohmann::json pkg_metadata;
            pkg_metadata["conda_pkg_format_version"] = 2;
            const auto metadata_file_path = tdir.path() / "metadata.json";
            std::ofstream metadata_file(metadata_file_path.std_path());
            metadata_file << pkg_metadata;
            metadata_file.close();

            create_archive(
                tdir.path(), out_file_abs, zip, 0, [](const std::string&) { return false; });
        }
    }

    void extract_archive(const fs::u8path& file, const fs::u8path& destination)
    {
        LOG_INFO << "Extracting " << file << " to " << destination;
        extraction_guard g(destination);

        // auto prev_path = fs::current_path();
        if (!fs::exists(destination))
        {
            fs::create_directories(destination);
        }
        fs::current_path(destination);

        struct archive* a;
        struct archive* ext;
        struct archive_entry* entry;
        int flags;
        int r;

        /* Select which attributes we want to restore. */
        flags = ARCHIVE_EXTRACT_TIME;
        flags |= ARCHIVE_EXTRACT_PERM;
        flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
        flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
        flags |= ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;
        flags |= ARCHIVE_EXTRACT_SPARSE;
        flags |= ARCHIVE_EXTRACT_UNLINK;

        a = archive_read_new();
        archive_read_support_format_tar(a);
        archive_read_support_format_zip(a);
        archive_read_support_filter_all(a);

        ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, flags);
        archive_write_disk_set_standard_lookup(ext);

        auto lock = LockFile::try_lock(file);
        r = archive_read_open_filename(a, file.string().c_str(), 10240);

        if (r != ARCHIVE_OK)
        {
            LOG_ERROR << "Error opening archive: " << archive_error_string(a);
            throw std::runtime_error(file.string() + " : Could not open archive for reading.");
        }

        for (;;)
        {
            if (is_sig_interrupted())
            {
                break;
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
        archive_read_close(a);
        archive_read_free(a);
        archive_write_close(ext);
        archive_write_free(ext);

        // fs::current_path(prev_path);
    }

    void extract_conda(const fs::u8path& file,
                       const fs::u8path& dest_dir,
                       const std::vector<std::string>& parts)
    {
        TemporaryDirectory tdir;
        extract_archive(file, tdir);

        auto fn = file.stem();

        auto metadata_path = tdir.path() / "metadata.json";
        if (fs::exists(metadata_path) && fs::file_size(metadata_path) != 0)
        {
            const auto condafile_path = tdir.path() / "metadata.json";
            std::ifstream condafile_meta(condafile_path.std_path());
            nlohmann::json j;
            condafile_meta >> j;

            if (j.find("conda_pkg_format_version") != j.end())
            {
                if (j["conda_pkg_format_version"] != 2)
                {
                    throw std::runtime_error("Can only read conda version 2 files.");
                }
            }
        }

        for (auto& part : parts)
        {
            std::stringstream ss;
            ss << part << "-" << fn.string() << ".tar.zst";
            extract_archive(tdir.path() / ss.str(), dest_dir);
        }
    }

    static fs::u8path extract_dest_dir(const fs::u8path& file)
    {
        if (ends_with(file.string(), ".tar.bz2"))
        {
            return file.string().substr(0, file.string().size() - 8);
        }
        else if (ends_with(file.string(), ".conda"))
        {
            return file.string().substr(0, file.string().size() - 6);
        }
        LOG_ERROR << "Unknown package format '" << file.string() << "'";
        throw std::runtime_error("Unknown package format.");
    }

    void extract(const fs::u8path& file, const fs::u8path& dest)
    {
        static std::mutex extract_mutex;
        std::lock_guard<std::mutex> lock(extract_mutex);

        if (ends_with(file.string(), ".tar.bz2"))
            extract_archive(file, dest);
        else if (ends_with(file.string(), ".conda"))
            extract_conda(file, dest);
        else
        {
            LOG_ERROR << "Unknown package format '" << file.string() << "'";
            throw std::runtime_error("Unknown package format.");
        }
    }

    fs::u8path extract(const fs::u8path& file)
    {
        fs::u8path dest_dir = extract_dest_dir(file);
        extract(file, dest_dir);
        return dest_dir;
    }

    void extract_subproc(const fs::u8path& file, const fs::u8path& dest)
    {
        std::vector<std::string> args;
        if (Context::instance().is_micromamba)
        {
            args = {
                get_self_exe_path().string(), "package", "extract", file.string(), dest.string()
            };
        }
        else
        {
            args = { "mamba-package", "extract", file.string(), dest.string() };
        }

        std::string out, err;
        LOG_DEBUG << "Running subprocess extraction '" << join(" ", args) << "'";
        auto [status, ec] = reproc::run(
            args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

        if (ec)
        {
            LOG_DEBUG << "Subprocess extraction exited with code " << ec << ", stdout: " << out
                      << ", stderr: " << err;
            LOG_DEBUG << "Running in-process extraction for '" << file.string() << "'";
            extract(file, dest);
        }
    }

    bool transmute(const fs::u8path& pkg_file, const fs::u8path& target, int compression_level)
    {
        TemporaryDirectory extract_dir;

        if (ends_with(pkg_file.string(), ".tar.bz2"))
        {
            extract_archive(pkg_file, extract_dir);
        }
        else if (ends_with(pkg_file.string(), ".conda"))
        {
            extract_conda(pkg_file, extract_dir);
        }
        else
        {
            throw std::runtime_error("Unknown package format (" + pkg_file.string() + ")");
        }

        create_package(extract_dir, target, compression_level);
        return true;
    }

    bool validate(const fs::u8path& pkg_folder)
    {
        auto safety_checks = Context::instance().safety_checks;
        if (safety_checks == VerificationLevel::kDisabled)
        {
            return true;
        }

        bool is_warn = safety_checks == VerificationLevel::kWarn;
        bool is_fail = safety_checks == VerificationLevel::kEnabled;
        bool full_validation = Context::instance().extra_safety_checks;

        try
        {
            auto paths_data = read_paths(pkg_folder);
            for (auto& p : paths_data)
            {
                fs::u8path full_path = pkg_folder / p.path;
                // "exists" follows symlink so if the symlink doesn't link to existing target it
                // will return false. There is such symlink in _openmp_mutex package. So if the file
                // is a symlink we don't want to follow the symlink. The "paths_data" should include
                // path of all the files and we shound't need to follow symlink.
                if (!(fs::exists(full_path) || fs::is_symlink(full_path)))
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
                        && !validate::file_size(full_path, p.size_in_bytes))
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
                        && !validate::sha256(full_path, p.sha256))
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
        catch (...)
        {
            LOG_WARNING << "Invalid package cache, could not read 'paths.json' from '"
                        << pkg_folder.string() << "'" << std::endl;
            return false;
        }
        return true;
    }
}  // namespace mamba
