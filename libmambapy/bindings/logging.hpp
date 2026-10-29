// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBAPY_BINDINGS_LOGGING_HPP
#define LIBMAMBAPY_BINDINGS_LOGGING_HPP

#include <functional>

#include <pybind11/pybind11.h>

#include <mamba/core/logging.hpp>

namespace mambapy
{
    void bind_submodule_logging(pybind11::module_ m);

    namespace details
    {

        using AnyLogHandlerConstructor = std::function<mamba::logging::AnyLogHandler(pybind11::object)>;

        auto
        register_anyloghandler_constructor(pybind11::type type_id, AnyLogHandlerConstructor constructor)
            -> void;
    }

    template <mamba::logging::LogHandler T>
    auto register_cpp_loghandler_type_for_anyloghandler() -> void
    {
        details::register_anyloghandler_constructor(
            pybind11::type::of<T>(),
            [](pybind11::object handler)
            { return mamba::logging::AnyLogHandler{ std::move(handler).cast<T>() }; }
        );
    }
}

#endif
