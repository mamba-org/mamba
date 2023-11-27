# Modified from bin2array, https://github.com/Jamesits/bin2array
# Distributed under MIT License:

# MIT License

# Copyright (c) 2017 James Swineson <github@public.swineson.me>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import argparse
import sys
from pathlib import Path


def bin2header(comment, data, var_name, extern=False):
    yield comment
    yield "#include <cstddef>"
    if extern:
        yield f"extern const char {var_name}[];"
        yield f"extern const std::size_t {var_name}_len;"
    yield f"const char {var_name}[] = {{"
    indent = "  "
    for i in range(0, len(data), 12):
        hex_chunk = ", ".join(f"0x{x:02x}" for x in data[i:][:12])
        yield indent + hex_chunk + ","
    yield indent + "0x00 // Terminating null byte"
    yield "};"
    yield f"const std::size_t {var_name}_len = {len(data)};"


def main():
    parser = argparse.ArgumentParser(description="Generate binary header output")
    parser.add_argument("-i", "--input", required=True, help="Input file", type=Path)
    parser.add_argument("-o", "--out", required=True, help="Output file", type=Path)
    parser.add_argument("-v", "--var", required=True, help="Variable name to use in file")
    parser.add_argument("-e", "--extern", action="store_true", help="Add 'extern' declaration")
    args = parser.parse_args()

    argv_pretty = " ".join(Path(arg).name if "/" in arg or "\\" in arg else arg for arg in sys.argv)
    comment = f"/* This file was generated using {argv_pretty} */"

    out = bin2header(comment, args.input.read_bytes(), args.var, args.extern)
    args.out.write_text("\n".join(out))


if __name__ == "__main__":
    sys.exit(main())
