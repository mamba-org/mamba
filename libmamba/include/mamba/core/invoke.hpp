#ifndef MAMBA_INVOKE_HPP
#define MAMBA_INVOKE_HPP

#include <functional>

#include <fmt/core.h>  // TODO: replace by `<format>` once available on all ci compilers

#include "mamba/core/error_handling.hpp"

namespace mamba
{

    template <typename Func, typename... Args>
    auto safe_invoke(Func&& func, Args&&... args)
        -> tl::expected<decltype(std::invoke(std::forward<Func>(func), std::forward<Args>(args)...)), mamba_error>
    {
        try
        {
            // If the callable is passed by being moved-in (r-value reference/temporary etc.)
            // we make sure that the lifetime of that callable doesn't go beyond this block.
            auto call = [&, callable = std::forward<Func>(func)]
            { return std::invoke(callable, std::forward<Args>(args)...); };
            using Result = decltype(call());

            if constexpr (std::is_void<Result>::value)
            {
                call();
                return {};
            }
            else
            {
                return call();
            }
        }
        catch (const std::exception& err)
        {
            return make_unexpected(
                fmt::format(
                    "invocation failed : `{}` threw exception `{}` : {}",
                    typeid(func).name(),
                    typeid(err).name(),
                    err.what()
                ),
                mamba_error_code::unknown
            );
        }
        catch (...)
        {
            return make_unexpected(
                fmt::format("invocation failed : `{}` threw an unknown error", typeid(func).name()),
                mamba_error_code::unknown
            );
        }
    }

}

#endif
