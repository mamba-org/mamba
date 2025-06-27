// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>
#include <variant>

#include <fmt/color.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "bind_utils.hpp"
#include "path_caster.hpp"

namespace mambapy
{
    // NOTE: These fmt "internals" are for internal use only.
    // External users should not use them in their projects.
    // Instead, rely on the public API or appropriate abstractions.
    // (not sure if they exist at the moment for this specific use case though)
    // cf. https://github.com/fmtlib/fmt/issues/4466
    bool fmt_is_rgb(const fmt::detail::color_type& color_type)
    {
#if FMT_VERSION >= 110200
        return !color_type.is_terminal_color();
#else
        return color_type.is_rgb;
#endif
    }

    void bind_submodule_utils(pybind11::module_ m)
    {
        namespace py = pybind11;

        py::enum_<fmt::emphasis>(m, "TextEmphasis")
            .value("Bold", fmt::emphasis::bold)
            .value("Faint", fmt::emphasis::faint)
            .value("Italic", fmt::emphasis::italic)
            .value("Underline", fmt::emphasis::underline)
            .value("Blink", fmt::emphasis::blink)
            .value("Reverse", fmt::emphasis::reverse)
            .value("Conceal", fmt::emphasis::conceal)
            .value("Strikethrough", fmt::emphasis::strikethrough)
            .def(py::init(&enum_from_str<fmt::emphasis>));
        py::implicitly_convertible<py::str, fmt::emphasis>();

        py::enum_<fmt::terminal_color>(m, "TextTerminalColor")
            .value("Black", fmt::terminal_color::black)
            .value("Red", fmt::terminal_color::red)
            .value("Green", fmt::terminal_color::green)
            .value("Yellow", fmt::terminal_color::yellow)
            .value("Blue", fmt::terminal_color::blue)
            .value("Magenta", fmt::terminal_color::magenta)
            .value("Cyan", fmt::terminal_color::cyan)
            .value("White", fmt::terminal_color::white)
            .value("BrightBlack", fmt::terminal_color::bright_black)
            .value("BrightRed", fmt::terminal_color::bright_red)
            .value("BrightGreen", fmt::terminal_color::bright_green)
            .value("BrightYellow", fmt::terminal_color::bright_yellow)
            .value("BrightBlue", fmt::terminal_color::bright_blue)
            .value("BrightMagenta", fmt::terminal_color::bright_magenta)
            .value("BrightCyan", fmt::terminal_color::bright_cyan)
            .value("BrightWhite", fmt::terminal_color::bright_white)
            .def(py::init(&enum_from_str<fmt::terminal_color>));
        py::implicitly_convertible<py::str, fmt::terminal_color>();

        py::class_<fmt::rgb>(m, "TextRGBColor")
            .def(py::init())
            .def(
                py::init<std::uint8_t, std::uint8_t, std::uint8_t>(),
                py::arg("red") = 0,
                py::arg("green") = 0,
                py::arg("blue") = 0
            )
            .def_readwrite("red", &fmt::rgb::r)
            .def_readwrite("green", &fmt::rgb::g)
            .def_readwrite("blue", &fmt::rgb::b)
            .def("__copy__", &copy<fmt::rgb>)
            .def("__deepcopy__", &deepcopy<fmt::rgb>, py::arg("memo"));

        using ColorType = std::variant<fmt::terminal_color, fmt::rgb>;
        py::class_<fmt::text_style>(m, "TextStyle")
            .def(py::init<>())
            .def(
                // We rely on ``std::optional`` and ``std::variant`` because
                // ``fmt::detail::color_type`` is semi-private and would lead to introducing
                // a new intermediary type.
                // The bitwise ``|`` syntax of fmt is also not Pythonic.
                // A more elegant option could be to implement a ``type_caster`` for ``color_type``.
                py::init(
                    [](std::optional<ColorType> fg,
                       std::optional<ColorType> bg,
                       std::optional<fmt::emphasis> em) -> fmt::text_style
                    {
                        auto out = fmt::text_style();
                        if (em)
                        {
                            out = fmt::text_style(em.value());
                        }
                        if (fg)
                        {
                            std::visit([&out](const auto& color) { out |= fmt::fg(color); }, fg.value());
                        }
                        if (bg)
                        {
                            std::visit([&out](const auto& color) { out |= fmt::bg(color); }, bg.value());
                        }
                        return out;
                    }
                ),
                py::arg("foreground") = std::nullopt,
                py::arg("background") = std::nullopt,
                py::arg("emphasis") = std::nullopt
            )
            .def_property_readonly(
                "foreground",
                [](const fmt::text_style& style) -> std::optional<ColorType>
                {
                    if (!style.has_foreground())
                    {
                        return std::nullopt;
                    }
                    const auto fg = style.get_foreground();
                    if (fmt_is_rgb(fg))
                    {
                        return { { fmt::rgb(fg.value.rgb_color) } };
                    }
                    return { { static_cast<fmt::terminal_color>(fg.value.term_color) } };
                }
            )
            .def_property_readonly(
                "background",
                [](const fmt::text_style& style) -> std::optional<ColorType>
                {
                    if (!style.has_background())
                    {
                        return std::nullopt;
                    }
                    const auto bg = style.get_background();
                    if (fmt_is_rgb(bg))
                    {
                        return { { fmt::rgb(bg.value.rgb_color) } };
                    }
                    return { { static_cast<fmt::terminal_color>(bg.value.term_color) } };
                }
            )
            .def_property_readonly(
                "emphasis",

                [](const fmt::text_style& style) -> std::optional<fmt::emphasis>
                {
                    if (!style.has_emphasis())
                    {
                        return std::nullopt;
                    }
                    return { style.get_emphasis() };
                }

            )
            .def("__copy__", &copy<fmt::text_style>)
            .def("__deepcopy__", &deepcopy<fmt::text_style>, py::arg("memo"));
    }
}
