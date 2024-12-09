#include <catch2/catch_all.hpp>

#include "mamba/core/activation.hpp"
#include "mamba/core/context.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("activation")
        {
            PosixActivator activator{ mambatests::context() };
            // std::cout << a.add_prefix_to_path("/home/wolfv/miniconda3", 0) <<
            // std::endl; std::cout << a.activate("/home/wolfv/miniconda3/", false) <<
            // std::endl;
        }

        TEST_CASE("Activator::get_default_env")
        {
            Context ctx;
            ctx.prefix_params.root_prefix = "/home/user/miniforge";
            PosixActivator a(ctx);
            REQUIRE(a.get_default_env("/home/user/miniforge") == "base");
            REQUIRE(a.get_default_env("/home/user/miniforge/envs/env") == "env");
            REQUIRE(a.get_default_env("/home/user/miniforge/envs/an.env") == "an.env");
            REQUIRE(a.get_default_env("/home/user/miniforge/envs/an-oth.er") == "an-oth.er");
            REQUIRE(a.get_default_env("/opt/envs/yet.an-oth.er") == "yet.an-oth.er");

            const fs::u8path& alternative_folder = "/opt/envs.d/env";
            REQUIRE(a.get_default_env(alternative_folder) == alternative_folder);

            const fs::u8path& alt_folder = "/home/user/some/env";
            REQUIRE(a.get_default_env(alt_folder) == alt_folder);
        }
    }
}  // namespace mamba
