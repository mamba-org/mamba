#include <system_error>

#include "thirdparty/filesystem.hpp"
#include <archive.h>
#include <archive_entry.h>

#include "util.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{
    void extract_conda()
    {
        throw std::runtime_error("Not implemented yet!");
    }

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
                return (ARCHIVE_OK);
            }
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(aw));
            }
            r = archive_write_data_block(aw, buff, size, offset);
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(aw));
            }
        }
    }

    void extract_tarball(const fs::path& file, const fs::path& destination)
    {
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
        // flags |= ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;
        flags |= ARCHIVE_EXTRACT_SPARSE;
        flags |= ARCHIVE_EXTRACT_UNLINK;

        a = archive_read_new();
        archive_read_support_format_tar(a);
        archive_read_support_filter_all(a);

        ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, flags);
        archive_write_disk_set_standard_lookup(ext);

        if ((r = archive_read_open_filename(a, file.c_str(), 10240)))
        {
            throw std::runtime_error("Could not open archive for reading.");
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

            // modify path to extract to directory
            // Either we do this here or we temporarily have to `chdir` to the dest dir
            const char* relative_file_path = archive_entry_pathname(entry);
            fs::path full_output_path = destination / relative_file_path;
            archive_entry_set_pathname(entry, full_output_path.c_str());

            r = archive_write_header(ext, entry);
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(a));
            }
            else
            {
                r = copy_data(a, ext);
                if (r < ARCHIVE_OK)
                {
                    throw std::runtime_error(archive_error_string(a));
                }
            }
            r = archive_write_finish_entry(ext);
            if (r < ARCHIVE_OK)
            {
                throw std::runtime_error(archive_error_string(a));
            }
        }
        archive_read_close(a);
        archive_read_free(a);
        archive_write_close(ext);
        archive_write_free(ext);
    }

    void extract(const fs::path& file)
    {
        std::string dest_dir = file;
        if (ends_with(dest_dir, ".tar.bz2"))
        {
            dest_dir = dest_dir.substr(0, dest_dir.size() - 8);
            extract_tarball(file, dest_dir);
        }
        else if (ends_with(dest_dir, ".conda"))
        {
            dest_dir = dest_dir.substr(0, dest_dir.size() - 6);
            extract_tarball(file, dest_dir);
        }
        else
        {
            throw std::runtime_error("Unknown file format.");
        }
    }
}