// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cstdlib>
#include <stdexcept>

#include <benchmark/benchmark.h>

#include "mamba/core/channel.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"

using namespace mamba;

namespace
{
    auto repodata_file() -> fs::u8path
    {
        const char* repodata_file_path = std::getenv("MAMBA_REPODATA_JSON");
        if (repodata_file_path == nullptr)
        {
            throw std::runtime_error("Expected MAMBA_REPODATA_JSON variable");
        }
        return { repodata_file_path };
    }

    void bench_repodata_libsolv(benchmark::State& state)
    {
        for (auto _ : state)
        {
            ChannelContext chan_ctx = {};
            MPool pool = { chan_ctx };
            MRepo repo = { pool, "mychannel", repodata_file(), {}, MRepo::RepodataParser::libsolv };
        }
    }
    BENCHMARK(bench_repodata_libsolv);

    void bench_repodata_mamba(benchmark::State& state)
    {
        for (auto _ : state)
        {
            ChannelContext chan_ctx = {};
            MPool pool = { chan_ctx };
            MRepo repo = { pool, "mychannel", repodata_file(), {}, MRepo::RepodataParser::mamba };
        }
    }
    BENCHMARK(bench_repodata_mamba);
}
