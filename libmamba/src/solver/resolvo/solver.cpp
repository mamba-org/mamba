#include <chrono>

#include <resolvo/resolvo.h>
#include <resolvo/resolvo_dependency_provider.h>

#include "mamba/solver/resolvo/database.hpp"
#include "mamba/solver/resolvo/solver.hpp"

namespace mamba::solver::resolvo
{
    using ::resolvo::String;
    using ::resolvo::Vector;
    using ::resolvo::VersionSetId;
    using ::resolvo::SolvableId;

    namespace
    {
        // TODO: Remove duplication with libsolv's
        auto make_request_cmp()
        {
            return util::make_variant_cmp(
                /** index_cmp= */
                [](auto lhs, auto rhs) { return lhs < rhs; },
                /** alternative_cmp= */
                [](const auto& lhs, const auto& rhs)
                {
                    using Itm = std::decay_t<decltype(lhs)>;
                    if constexpr (!std::is_same_v<Itm, Request::UpdateAll>)
                    {
                        return lhs.spec.name().str() < rhs.spec.name().str();
                    }
                    return false;
                }
            );
        }
    }

    auto Solver::solve(PackageDatabase& mpool, Request&& request) -> expected_t<Outcome>
    {
        if (request.flags.order_request)
        {
            std::sort(request.jobs.begin(), request.jobs.end(), make_request_cmp());
        }
        return solve_impl(mpool, request);
    }

    auto Solver::solve(PackageDatabase& mpool, const Request& request) -> expected_t<Outcome>
    {
        if (request.flags.order_request)
        {
            auto sorted_request = request;
            std::sort(sorted_request.jobs.begin(), sorted_request.jobs.end(), make_request_cmp());
            return solve_impl(mpool, sorted_request);
        }
        return solve_impl(mpool, request);
    }

    auto Solver::solve_impl(PackageDatabase& mpool, const Request& request) -> expected_t<Outcome>
    {
        std::vector<MatchSpec> specs;

        for (const auto& job : request.jobs)
        {
            std::visit(
                [&](const auto& j)
                {
                    using A = std::decay_t<decltype(j)>;
                    if constexpr (std::is_same_v<A, Request::Install>)
                    {
                        specs.push_back(j.spec);
                    }
                    else if constexpr (std::is_same_v<A, Request::Remove>)
                    {
                        specs.push_back(j.spec);
                    }
                    else if constexpr (std::is_same_v<A, Request::Update>)
                    {
                        specs.push_back(j.spec);
                    }
                    else
                    {
                        // TODO: support other job types
                        throw std::runtime_error("Unsupported job type");
                    }
                },
                job
            );
        }

        resolvo::Vector<resolvo::VersionSetId> requirements;
        for (const auto& spec : specs)
        {
            // TODO: avoid converting the spec to a string
            requirements.push_back(mpool.alloc_version_set(spec.str()));
        }

        resolvo::Vector<resolvo::VersionSetId> constraints = {};
        resolvo::Vector<resolvo::SolvableId> result;

        auto tick_resolvo = std::chrono::steady_clock::now();
        String reason = ::resolvo::solve(mpool, requirements, constraints, result);
        auto tack_resolvo = std::chrono::steady_clock::now();

        if (reason == "")  // success
        {
            std::vector<PackageInfo> resolvo_resolution;
            for (auto solvable_id : result)
            {
                const PackageInfo& package_info = mpool.get_solvable(solvable_id);
                // Skip virtual package (i.e. whose `package_info.name` starts with "__")
                if (package_info.name.find("__") != 0)
                {
                    resolvo_resolution.push_back(package_info);
                }
            }

            std::sort(
                resolvo_resolution.begin(),
                resolvo_resolution.end(),
                [&](const PackageInfo& a, const PackageInfo& b) { return a.name < b.name; }
            );

            Solution solution;

            solution.actions.reserve(resolvo_resolution.size());
            for (const auto& package_info : resolvo_resolution)
            {
                solution.actions.emplace_back(Solution::Install{ package_info });
            }

            return solution;
        }
        return UnSolvable(std::string(reason.data()));
    }


}
