// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include <catch2/catch_all.hpp>

#include "mamba/api/clean.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/core/cache_paths.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/environment.hpp"

#include "mambatests.hpp"

namespace mamba
{
    TEST_CASE("clean all removes user shard cache directory")
    {
        auto restore = mambatests::EnvironmentCleaner(mambatests::CleanMambaEnv{});
        const auto tmp_home = TemporaryDirectory();
        const auto tmp_root_prefix = TemporaryDirectory();
        const auto tmp_cache_home = TemporaryDirectory();
        const auto tmp_pkgs_dir = TemporaryDirectory();

        util::set_env("HOME", tmp_home.path().string());
        util::set_env("MAMBA_ROOT_PREFIX", tmp_root_prefix.path().string());
        util::set_env("CONDA_PKGS_DIRS", tmp_pkgs_dir.path().string());
        util::set_env("XDG_CACHE_HOME", tmp_cache_home.path().string());

        const auto shard_cache_dir = tmp_cache_home.path() / cache_paths::conda_pkgs_relative
                                     / cache_paths::cache_shards_relative;
        fs::create_directories(shard_cache_dir);
        const auto shard_file = shard_cache_dir / "dummy.msgpack.zst";
        {
            std::ofstream out(shard_file.std_path());
            out << "dummy";
        }
        REQUIRE(fs::exists(shard_file));

        auto& ctx = mambatests::context();
        Configuration config{ ctx };
        clean(config, MAMBA_CLEAN_ALL);

        REQUIRE_FALSE(fs::exists(shard_cache_dir));
    }
}  // namespace mamba
