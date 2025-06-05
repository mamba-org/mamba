#ifdef _WIN32

// Catch compiled on `conda-forge` for MSVC doesn't support outputting `string_view`.
// So we have to define StringMaker for it ourselves.
// The declaration is present though, so this only causes link errors.

#include <string>
#include <string_view>

#include <catch2/catch_tostring.hpp>

namespace Catch
{

    std::string StringMaker<std::string_view>::convert(std::string_view str)
    {
        return std::string(str);
    }

}

#endif
