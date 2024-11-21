// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/environments_manager.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/path_manip.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("all_envs")
        {
            EnvironmentsManager e{ mambatests::context() };
            auto prefixes = e.list_all_known_prefixes();
            // Test registering env without `conda-meta/history` file
            e.register_env(util::expand_home("~/some/env"));
            auto new_prefixes = e.list_all_known_prefixes();
            // the prefix should be cleaned out, because it doesn't have the
            // `conda-meta/history` file
            REQUIRE(new_prefixes.size() == prefixes.size();

            // Create an env containing `conda-meta/history` file
            // and test register/unregister
            auto prefix = fs::u8path(util::expand_home("~/some_test_folder/other_env"));
            path::touch(prefix / "conda-meta" / "history", true);

            e.register_env(prefix);
            new_prefixes = e.list_all_known_prefixes();
            REQUIRE(new_prefixes.size() == prefixes.size() + 1);

            e.unregister_env(prefix);
            new_prefixes = e.list_all_known_prefixes();
            REQUIRE(new_prefixes.size() == prefixes.size();

            // Add another file in addition to `conda-meta/history`
            // and test register/unregister
            path::touch(prefix / "conda-meta" / "other_file", true);

            e.register_env(prefix);
            new_prefixes = e.list_all_known_prefixes();
            REQUIRE(new_prefixes.size() == prefixes.size() + 1);

            e.unregister_env(prefix);
            new_prefixes = e.list_all_known_prefixes();
            // Shouldn't unregister because `conda-meta/other_file`
            // is there
            REQUIRE(new_prefixes.size() == prefixes.size() + 1);

            // Remove test directory
            fs::remove_all(util::expand_home("~/some_test_folder"));
        }
    }
}  // namespace mamba
