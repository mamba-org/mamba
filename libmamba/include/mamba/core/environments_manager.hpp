// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_ENVIRONMENT_MANAGER
#define MAMBA_CORE_ENVIRONMENT_MANAGER

#include <string>
#include <set>

#include "fsutil.hpp"

namespace mamba
{
    const char PREFIX_MAGIC_FILE[] = "conda-meta/history";

    bool is_conda_environment(const fs::path& prefix);

    class EnvironmentsManager
    {
    public:
        void register_env(const fs::path& location);
        void unregister_env(const fs::path& location);
        std::set<fs::path> list_all_known_prefixes();

    private:
        std::set<std::string> clean_environments_txt(const fs::path& env_txt_file,
                                                     const fs::path& location);
        std::string remove_trailing_slash(std::string p);
        fs::path get_environments_txt_file(const fs::path& home) const;
    };
}  // namespace mamba

#endif
