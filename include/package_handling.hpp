#ifndef MAMBA_PACKAGE_HANDLING_HPP
#define MAMBA_PACKAGE_HANDLING_HPP

#include <system_error>
#include "thirdparty/filesystem.hpp"

#include <archive.h>
#include <archive_entry.h>

#include "util.hpp"
#include "nlohmann/json.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{
    int copy_data(archive *ar, archive *aw);
    void extract_archive(const fs::path& file, const fs::path& destination);
    void extract_conda(const fs::path& file, const fs::path& dest_dir, const std::vector<std::string>& parts = {"info", "pkg"});
    fs::path strip_package_name(const std::string& file);
    fs::path extract(const fs::path& file);
}

#endif // MAMBA_PACKAGE_HANDLING_HPP
