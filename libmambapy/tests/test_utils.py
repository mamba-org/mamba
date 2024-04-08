import copy

import pytest

import libmambapy


def test_import_submodule():
    import libmambapy.utils as utils

    # Dummy execution
    _p = utils.TextEmphasis


def test_import_recursive():
    import libmambapy as mamba

    # Dummy execution
    _p = mamba.utils.TextEmphasis


def test_TextEmphasis():
    TextEmphasis = libmambapy.utils.TextEmphasis

    assert TextEmphasis.Bold.name == "Bold"
    assert TextEmphasis.Faint.name == "Faint"
    assert TextEmphasis.Italic.name == "Italic"
    assert TextEmphasis.Underline.name == "Underline"
    assert TextEmphasis.Blink.name == "Blink"
    assert TextEmphasis.Reverse.name == "Reverse"
    assert TextEmphasis.Conceal.name == "Conceal"
    assert TextEmphasis.Strikethrough.name == "Strikethrough"

    assert TextEmphasis("Italic") == TextEmphasis.Italic

    with pytest.raises(KeyError):
        # No parsing, explicit name
        TextEmphasis(" bold")


def test_TextTerminalColor():
    TextTerminalColor = libmambapy.utils.TextTerminalColor

    assert TextTerminalColor.Black.name == "Black"
    assert TextTerminalColor.Red.name == "Red"
    assert TextTerminalColor.Green.name == "Green"
    assert TextTerminalColor.Yellow.name == "Yellow"
    assert TextTerminalColor.Blue.name == "Blue"
    assert TextTerminalColor.Magenta.name == "Magenta"
    assert TextTerminalColor.Cyan.name == "Cyan"
    assert TextTerminalColor.White.name == "White"
    assert TextTerminalColor.BrightBlack.name == "BrightBlack"
    assert TextTerminalColor.BrightRed.name == "BrightRed"
    assert TextTerminalColor.BrightGreen.name == "BrightGreen"
    assert TextTerminalColor.BrightYellow.name == "BrightYellow"
    assert TextTerminalColor.BrightBlue.name == "BrightBlue"
    assert TextTerminalColor.BrightMagenta.name == "BrightMagenta"
    assert TextTerminalColor.BrightCyan.name == "BrightCyan"
    assert TextTerminalColor.BrightWhite.name == "BrightWhite"

    assert TextTerminalColor("Red") == TextTerminalColor.Red

    with pytest.raises(KeyError):
        # No parsing, explicit name
        TextTerminalColor("red ")


def test_TextRGBColor():
    TextRGBColor = libmambapy.utils.TextRGBColor

    color = TextRGBColor(red=11, blue=33, green=22)

    # Getters
    assert color.red == 11
    assert color.green == 22
    assert color.blue == 33

    # Setters
    color.red = 1
    color.green = 2
    color.blue = 3
    assert color.red == 1
    assert color.green == 2
    assert color.blue == 3

    # Copy
    other = copy.deepcopy(color)
    assert other is not color
    assert other.red == color.red


def test_TextStyle():
    TextTerminalColor = libmambapy.utils.TextTerminalColor
    TextRGBColor = libmambapy.utils.TextRGBColor
    TextEmphasis = libmambapy.utils.TextEmphasis
    TextStyle = libmambapy.utils.TextStyle

    style = TextStyle()
    assert style.foreground is None
    assert style.background is None
    assert style.emphasis is None

    style = TextStyle(foreground="Red", background=TextRGBColor(red=123), emphasis="Underline")
    assert style.foreground == TextTerminalColor.Red
    assert style.background.red == 123
    assert style.emphasis == TextEmphasis.Underline

    # Copy
    other = copy.deepcopy(style)
    assert other is not style
    assert other.emphasis == style.emphasis
