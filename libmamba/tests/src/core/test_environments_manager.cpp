// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/filesystem/u8path.hpp"

#include "mambatests.hpp"

namespace mamba
{
    TEST_SUITE("env_manager")
    {
        TEST_CASE("all_envs")
        {
            EnvironmentsManager e{ mambatests::context() };
            auto prefixes = e.list_all_known_prefixes();
            // Test registering env without `conda-meta/history` file
            e.register_env(env::expand_user("~/some/env"));
            auto new_prefixes = e.list_all_known_prefixes();
            // the prefix should be cleaned out, because it doesn't have the
            // `conda-meta/history` file
            CHECK_EQ(new_prefixes.size(), prefixes.size());

            // Create an env containing `conda-meta/history` file
            // and test register/unregister
            auto prefix = env::expand_user("~/some_test_folder/other_env");
            path::touch(prefix / "conda-meta" / "history", true);

            e.register_env(prefix);
            new_prefixes = e.list_all_known_prefixes();
            CHECK_EQ(new_prefixes.size(), prefixes.size() + 1);

            e.unregister_env(prefix);
            new_prefixes = e.list_all_known_prefixes();
            CHECK_EQ(new_prefixes.size(), prefixes.size());

            // Add another file in addition to `conda-meta/history`
            // and test register/unregister
            path::touch(prefix / "conda-meta" / "other_file", true);

            e.register_env(prefix);
            new_prefixes = e.list_all_known_prefixes();
            CHECK_EQ(new_prefixes.size(), prefixes.size() + 1);

            e.unregister_env(prefix);
            new_prefixes = e.list_all_known_prefixes();
            // Shouldn't unregister because `conda-meta/other_file`
            // is there
            CHECK_EQ(new_prefixes.size(), prefixes.size() + 1);

            // Remove test directory
            fs::remove_all(env::expand_user("~/some_test_folder"));
        }
    }
}  // namespace mamba
