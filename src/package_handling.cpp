// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <archive.h>
#include <archive_entry.h>

#include <sstream>

#include "nlohmann/json.hpp"
#include "mamba/package_handling.hpp"
#include "mamba/package_paths.hpp"
#include "mamba/output.hpp"
#include "mamba/thread_utils.hpp"
#include "mamba/util.hpp"
#include "mamba/validate.hpp"

namespace mamba
{
    class extraction_guard
    {
    public:
        explicit extraction_guard(const fs::path& file)
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
        const fs::path& m_file;
    };

    static int copy_data(archive* ar, archive* aw)
    {
        int r;
        const void* buff;
        std::size_t size;
        la_int64_t offset;

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
    void create_archive(const fs::path& directory,
                        const fs::path& destination,
                        compression_algorithm ca,
                        int compression_level,
                        bool (*filter)(const std::string&))
    {
        int r;
        struct archive* a;

        extraction_guard g(destination);

        fs::path abs_out_path = fs::absolute(destination);
        a = archive_write_new();
        if (ca == compression_algorithm::bzip2)
        {
            archive_write_set_format_gnutar(a);
            archive_write_set_format_pax_restricted(a);  // Note 1
            archive_write_add_filter_bzip2(a);
        }
        if (ca == compression_algorithm::zip)
        {
            std::string comp_level
                = std::string("zip:compression-level=") + std::to_string(compression_level);
            archive_write_set_format_zip(a);
            archive_write_set_options(a, comp_level.c_str());
        }
        if (ca == compression_algorithm::zstd)
        {
            archive_write_set_format_gnutar(a);
            archive_write_set_format_pax_restricted(a);  // Note 1
            archive_write_add_filter_zstd(a);
            std::string comp_level
                = std::string("zstd:compression-level=") + std::to_string(compression_level);
            archive_write_set_options(a, comp_level.c_str());
        }

        archive_write_open_filename(a, abs_out_path.c_str());

        auto prev_path = fs::current_path();
        if (!fs::exists(directory))
        {
            throw std::runtime_error("Directory does not exist.");
        }
        fs::current_path(directory);

        for (auto& dir_entry : fs::recursive_directory_iterator("."))
        {
            if (dir_entry.is_directory())
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
    void create_package(const fs::path& directory, const fs::path& out_file, int compression_level)
    {
        fs::path out_file_abs = fs::absolute(out_file);
        if (ends_with(out_file.string(), ".tar.bz2"))
        {
            create_archive(
                directory, out_file_abs, bzip2, compression_level, [](const std::string&) {
                    return false;
                });
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
            std::ofstream metadata_file(tdir.path() / "metadata.json");
            metadata_file << pkg_metadata;
            metadata_file.close();

            create_archive(
                tdir.path(), out_file_abs, zip, 0, [](const std::string&) { return false; });
        }
    }

    void extract_archive(const fs::path& file, const fs::path& destination)
    {
        LOG_INFO << "Extracting " << file << " to " << destination;
        extraction_guard g(destination);

        auto prev_path = fs::current_path();
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

        if ((r = archive_read_open_filename(a, file.c_str(), 10240)))
        {
            throw std::runtime_error(std::string(file) + ": Could not open archive for reading.");
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

        fs::current_path(prev_path);
    }

    void extract_conda(const fs::path& file,
                       const fs::path& dest_dir,
                       const std::vector<std::string>& parts)
    {
        TemporaryDirectory tdir;
        extract_archive(file, tdir);

        auto fn = file.stem();

        auto metadata_path = tdir.path() / "metadata.json";
        if (fs::exists(metadata_path) && fs::file_size(metadata_path) != 0)
        {
            std::ifstream condafile_meta(tdir.path() / "metadata.json");
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
            ss << part << "-" << fn.c_str() << ".tar.zst";
            extract_archive(tdir.path() / ss.str(), dest_dir);
        }
    }

    fs::path extract(const fs::path& file)
    {
        std::string dest_dir = file;
        if (ends_with(dest_dir, ".tar.bz2"))
        {
            dest_dir = dest_dir.substr(0, dest_dir.size() - 8);
            extract_archive(file, dest_dir);
        }
        else if (ends_with(dest_dir, ".conda"))
        {
            dest_dir = dest_dir.substr(0, dest_dir.size() - 6);
            extract_conda(file, dest_dir);
        }
        else
        {
            throw std::runtime_error("Unknown package format (" + file.string() + ")");
        }
        return dest_dir;
    }

    bool transmute(const fs::path& pkg_file, const fs::path& target, int compression_level)
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

    bool validate(const fs::path& pkg_folder)
    {
        auto full_validation = Context::instance().extra_safety_checks != VerificationLevel::NONE;
        try
        {
            auto paths_data = read_paths(pkg_folder);
            for (auto& p : paths_data)
            {
                fs::path full_path = pkg_folder / p.path;
                if (!fs::exists(full_path))
                {
                    LOG_WARNING << "Missing file from package cache: " << full_path;
                    return false;
                }
                // old packages don't have paths.json with validation information
                if (p.size_in_bytes != 0 && Context::instance().extra_safety_checks)
                {
                    bool is_invalid = false;
                    if (p.path_type != PathType::SOFTLINK
                        && !validate::file_size(full_path, p.size_in_bytes))
                    {
                        LOG_WARNING << "Size incorrect, file modified in package cache "
                                    << full_path;
                        is_invalid = true;
                        if (Context::instance().extra_safety_checks != VerificationLevel::WARN)
                        {
                            return false;
                        }
                    }
                    if (!is_invalid && full_validation && p.path_type != PathType::SOFTLINK
                        && !validate::sha256(full_path, p.sha256))
                    {
                        LOG_WARNING << "SHA256 checksum incorrect, file modified in package cache "
                                    << full_path;
                        if (Context::instance().extra_safety_checks != VerificationLevel::WARN)
                        {
                            return false;
                        }
                    }
                }
                LOG_INFO << "Validated " << p.path;
            }
        }
        catch (...)
        {
            LOG_WARNING << "Could not read paths.json from " << pkg_folder << std::endl;
            return false;
        }
        return true;
    }
}  // namespace mamba
