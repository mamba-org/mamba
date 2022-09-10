#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
    bool is_process_name_running(const std::string& name);
    std::string generate_unique_process_name(std::string_view program_name);
    const fs::u8path& proc_dir();
    std::unique_ptr<LockFile> lock_proc_dir();

    nlohmann::json get_all_running_processes_info(
        const std::function<bool(const nlohmann::json&)>& filter
        = std::function<bool(const nlohmann::json&)>());
    bool is_process_name_running(const std::string& name);
}
