// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/repo_checker_store.hpp"
#include "mamba/core/subdirdata.hpp"

namespace mamba
{

    auto RepoCheckerStore::make(  //
        const Context& ctx,
        ChannelContext& cc,
        MultiPackageCache& caches
    ) -> RepoCheckerStore
    {
        if (ctx.validation_params.verify_artifacts ==

            VerificationLevel::Disabled)
        {
            return RepoCheckerStore({});
        }

        auto repo_checkers = repo_checker_list();
        repo_checkers.reserve(ctx.validation_params.trusted_channels.size());
        for (const auto& location : ctx.validation_params.trusted_channels)
        {
            for (auto& chan : cc.make_channel(location))
            {
                // Parametrization
                auto url = chan.url().str(specs::CondaURL::Credentials::Show);
                auto url_id = cache_name_from_url(url);
                auto ref_path = ctx.prefix_params.root_prefix / "etc" / "trusted-repos" / url_id;
                auto checker = RepoChecker(
                    ctx,
                    std::move(url),
                    std::move(ref_path),
                    caches.first_writable_path() / "cache" / url_id
                );

                // Initialization
                fs::create_directories(checker.cache_path());
                checker.generate_index_checker();

                repo_checkers.emplace_back(std::move(chan), std::move(checker));
            }
        }
        return RepoCheckerStore(std::move(repo_checkers));
    }

    RepoCheckerStore::RepoCheckerStore(repo_checker_list checkers)
        : m_repo_checkers(std::move(checkers))
    {
    }

    auto RepoCheckerStore::find_checker(const Channel& chan) const -> const RepoChecker*
    {
        for (auto& [candidate_chan, checker] : m_repo_checkers)
        {
            if (candidate_chan.contains_equivalent(chan))
            {
                return &checker;
            }
        }
        return nullptr;
    }

    auto RepoCheckerStore::contains_checker(const Channel& chan) const -> bool
    {
        return find_checker(chan) != nullptr;
    }

    auto RepoCheckerStore::at_checker(const Channel& chan) const -> const RepoChecker&
    {
        if (auto ptr = find_checker(chan))
        {
            return *ptr;
        }
        throw std::range_error("Checker not found");
    }
}
