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


def bin2header(data, var_name="var"):
    out = []
    out.append("char {var_name}[] = {{".format(var_name=var_name))
    elems = [data[i : i + 12] for i in range(0, len(data), 12)]
    for i, x in enumerate(elems):
        line = ", ".join(["0x{val:02x}".format(val=c) for c in x])
        out.append(
            "  {line}{end_comma}".format(
                line=line, end_comma="," if i < len(elems) - 1 else ""
            )
        )
    out.append("};")
    out.append(
        "std::size_t {var_name}_len = {data_len};".format(
            var_name=var_name, data_len=len(data)
        )
    )
    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(description="Generate binary header output")
    parser.add_argument("-i", "--input", required=True, help="Input file")
    parser.add_argument("-o", "--out", required=True, help="Output file")
    parser.add_argument(
        "-v", "--var", required=True, help="Variable name to use in file"
    )

    args = parser.parse_args()
    if not args:
        return 1

    with open(args.input, "rb") as f:
        data = f.read()

    out = bin2header(data, args.var)
    with open(args.out, "w") as f:
        f.write(out)

    return 0


if __name__ == "__main__":
    sys.exit(main())
