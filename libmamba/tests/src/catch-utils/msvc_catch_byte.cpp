#ifdef _WIN32

// Catch compiled on `conda-forge` for MSVC doesn't support outputting `std::byte`.
// So we have to define StringMaker for it ourselves.
// The declaration is present though, so this only causes link errors.

#include <string>

#include <catch2/catch_tostring.hpp>
#include <fmt/format.h>

namespace Catch
{

    std::string StringMaker<std::byte>::convert(std::byte value)
    {
        return fmt::format("{}", value);
    }

}

#endif
