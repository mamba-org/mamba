#include "mamba/core/channel.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/package_cache.hpp"

namespace mamba
{
    expected_t<void, mamba_aggregated_error> load_channels(MPool& pool,
                                                           MultiPackageCache& package_caches,
                                                           int is_retry);
}
