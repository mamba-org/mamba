#include <filesystem>

namespace fs = std::filesystem;

namespace path
{
    bool lexists(const fs::path& path)
    {
        return fs::status_known(fs::symlink_status(path));
    } 
}