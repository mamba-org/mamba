#ifndef MAMBA_PARSING_HPP
#define MAMBA_PARSING_HPP

#include <iostream>
#include <array>
#include <string>
#include <stdexcept>
#ifndef _WIN32
    #include <string_view>
#else
    #include "thirdparty/string_view.hpp"
    namespace std {
        using namespace nonstd;
    }
#endif
#include <cassert>

extern "C"
{
    #include "solv/pool.h"
    #include "solv/repo.h"
    #include "solv/queue.h"
    #include "solv/solver.h"
}

#include "thirdparty/pystring/pystring.hpp"

struct parsed_relation {
    int relation = -1;
    bool fuzzy = false;
    std::string_view version;
};

// TODO better concat
std::string normalize_version(std::string_view version, int build_number);
std::string normalize_version(std::string_view version, int build_number, std::string_view build);
Id parse_to_relation(const std::string_view& dep, Pool* pool);

#endif
