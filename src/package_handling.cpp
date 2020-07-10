// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <sstream>

#include <archive.h>
#include <archive_entry.h>

#include "util.hpp"
#include "nlohmann/json.hpp"

#include "package_handling.hpp"
#include "output.hpp"

namespace mamba
{
    static int copy_data(archive *ar, archive *aw)
    {
        int r;
        const void *buff;
        size_t size;
        la_int64_t offset;

        while (true)
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

    void extract_archive(const fs::path& file, const fs::path& destination)
    {
        LOG_INFO << "Extracting " << file << " to " << destination;

        auto prev_path = fs::current_path();
        if (!fs::exists(destination))
        {
            fs::create_directories(destination);
        }
        fs::current_path(destination);

        struct archive *a;
        struct archive *ext;
        struct archive_entry *entry;
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
            if (r < ARCHIVE_OK)
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

    void extract_conda(const fs::path& file, const fs::path& dest_dir, const std::vector<std::string>& parts)
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

    void split_package_extension(const std::string& file, std::string& name, std::string& extension)
    {
        if (ends_with(file, ".conda"))
        {
            name = file.substr(0, file.size() - 6);
            extension = ".conda";
        }
        else if (ends_with(file, ".tar.bz2"))
        {
            name = file.substr(0, file.size() - 8);
            extension = ".tar.bz2";
        }
        else if (ends_with(file, ".json"))
        {
            name = file.substr(0, file.size() - 5);
            extension = ".json";
        }
        else
        {
            name = file;
            extension = "";
        }
    }

    fs::path strip_package_extension(const std::string& file)
    {
        std::string name, extension;
        split_package_extension(file, name, extension);

        if (extension == "")
        {
            throw std::runtime_error("Don't know how to handle " + file);
        }

        return name;
    }

    fs::path extract(const fs::path& file)
    {
        std::string dest_dir = file;
        if (ends_with(dest_dir, ".tar.bz2"))
        {
            dest_dir = dest_dir.substr(0, dest_dir.size() - 8);
            extract_archive(file, dest_dir);
            return dest_dir;
        }
        else if (ends_with(dest_dir, ".conda"))
        {
            dest_dir = dest_dir.substr(0, dest_dir.size() - 6);
            extract_conda(file, dest_dir);
            return dest_dir;
        }
        else
        {
            throw std::runtime_error("Unknown file format (" + std::string(file) + ")");
        }
    }
}

