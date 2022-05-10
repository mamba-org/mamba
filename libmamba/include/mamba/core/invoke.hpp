#ifndef MAMBA_INVOKE_HPP
#define MAMBA_INVOKE_HPP

#include <functional>

#include "mamba/core/error_handling.hpp"

namespace mamba
{

    template <typename Func, typename... Args>
    auto safe_invoke(Func&& func, Args&&... args)
        -> tl::expected<decltype(std::invoke(std::forward<Func>(func),
                                             std::forward<Args>(args)...)),
                        mamba_error>
    {
        auto call
            = [&] { return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...); };
        using Result = decltype(call());
        try
        {
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
        catch (const std::runtime_error& err)
        {
            return make_unexpected(std::string("callback invocation failed : ") + err.what(),
                                   mamba_error_code::unknown);
        }
        catch (...)
        {
            return make_unexpected("callback invocation failed : unknown error",
                                   mamba_error_code::unknown);
        }
    }

}

#endif
