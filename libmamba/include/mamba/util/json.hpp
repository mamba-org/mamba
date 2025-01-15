// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_JSON_HPP
#define MAMBA_UTIL_JSON_HPP


#include <nlohmann/json.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN

template <typename T>
struct adl_serializer<std::optional<T>>
{
    static void to_json(json& j, const std::optional<T>& opt)
    {
        if (opt.has_value())
        {
            j = opt.value();
        }
        else
        {
            j = nullptr;
        }
    }

    static void from_json(const json& j, std::optional<T>& opt)
    {
        if (!j.is_null())
        {
            opt = j.template get<T>();
        }
        else
        {
            opt = std::nullopt;
        }
    }
};

NLOHMANN_JSON_NAMESPACE_END

namespace mamba::util
{
    template <typename Json, std::size_t N, typename T>
    void deserialize_maybe_missing(Json&& j, const char (&name)[N], T& t)
    {
        if (j.contains(name))
        {
            t = std::forward<Json>(j)[name].template get<T>();
        }
        else
        {
            t = {};
        }
    }
}

#endif
