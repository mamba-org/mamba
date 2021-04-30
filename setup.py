# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

# -*- coding: utf-8 -*-

import os
import sys

import setuptools
from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

here = os.path.dirname(os.path.abspath(__file__))

version_ns = {}
with open(os.path.join(here, "mamba", "_version.py")) as f:
    exec(f.read(), {}, version_ns)

__version__ = version_ns["__version__"]

with open("include/mamba/core/version.hpp.in", "r") as fi:
    cpp_version_template = fi.read()

v = version_ns["version_info"]
cpp_version_template = (
    cpp_version_template.replace("@MAMBA_VERSION_MAJOR@", str(v[0]))
    .replace("@MAMBA_VERSION_MINOR@", str(v[1]))
    .replace("@MAMBA_VERSION_PATCH@", str(v[2]))
)

with open("include/mamba/core/version.hpp", "w") as fo:
    fo.write(cpp_version_template)


class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked."""

    def __init__(self, user=False):
        self.user = user

    def __str__(self):
        import pybind11

        return pybind11.get_include(self.user)


if sys.platform.startswith("win"):
    libsolv_prefix = os.path.join(sys.prefix, "Library\\")
else:
    libsolv_prefix = sys.prefix

print("Looking for libsolv in: ", libsolv_prefix)

extra_link_args = []
if sys.platform == "darwin":
    extra_link_args = ["-Wl,-rpath", "-Wl,%s" % os.path.abspath(libsolv_prefix)]

library_dir = []
if sys.platform == "win32":
    try:
        conda_prefix = os.getenv("CONDA_PREFIX")
        if not conda_prefix:
            conda_prefix = os.getenv("MINICONDA")
        if not conda_prefix:
            raise RuntimeError("No conda prefix found")

        library_dir = [os.path.join(conda_prefix, "Library\\lib\\")]
        print("Looking for libsolv library in ", library_dir)
    except Exception:
        print("could not find conda prefix")

    CURL_LIB = "libcurl"
    CRYPTO_LIB = "libcrypto"
else:
    CURL_LIB = "curl"
    CRYPTO_LIB = "crypto"

libraries = ["archive", "solv", "solvext", "reproc++", CURL_LIB, CRYPTO_LIB]
if sys.platform == "win32":
    libraries += ["advapi32", "libsodium"]

ext_modules = [
    Extension(
        "mamba.mamba_api",
        [
            "src/mamba/py_interface.cpp",
            "src/core/activation.cpp",
            "src/core/channel.cpp",
            "src/core/context.cpp",
            "src/core/fetch.cpp",
            "src/core/history.cpp",
            "src/core/match_spec.cpp",
            "src/core/output.cpp",
            "src/core/package_handling.cpp",
            "src/core/package_cache.cpp",
            "src/core/package_paths.cpp",
            "src/core/prefix_data.cpp",
            "src/core/package_info.cpp",
            "src/core/pool.cpp",
            "src/core/query.cpp",
            "src/core/repo.cpp",
            "src/core/solver.cpp",
            "src/core/shell_init.cpp",
            "src/core/subdirdata.cpp",
            "src/core/thread_utils.cpp",
            "src/core/transaction.cpp",
            "src/core/transaction_context.cpp",
            "src/core/url.cpp",
            "src/core/util.cpp",
            "src/core/validate.cpp",
            "src/core/version.cpp",
            "src/core/link.cpp",
        ],
        include_dirs=[
            get_pybind_include(),
            get_pybind_include(user=True),
            os.path.join(libsolv_prefix, "include"),
            "include/",
            "src/",
        ],
        library_dirs=library_dir,
        extra_link_args=extra_link_args,
        libraries=libraries,
        language="c++",
    )
]


# As of Python 3.6, CCompiler has a `has_flag` method.
# cf http://bugs.python.org/issue26689
def has_flag(compiler, flagname):
    """Return a boolean indicating whether a flag name is supported on
    the specified compiler.
    """
    import tempfile

    with tempfile.NamedTemporaryFile("w", suffix=".cpp") as f:
        f.write("int main (int argc, char **argv) { return 0; }")
        try:
            compiler.compile([f.name], extra_postargs=[flagname])
        except setuptools.distutils.errors.CompileError:
            return False
    return True


class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""

    c_opts = {
        "msvc": ["/EHsc", "/std:c++17", "/Ox", "/DNOMINMAX"],
        "unix": ["-std=c++17", "-O3"],
    }

    def build_extensions(self):
        ct = self.compiler.compiler_type

        if sys.platform == "darwin":
            self.c_opts["unix"] += ["-stdlib=libc++", "-mmacosx-version-min=10.7"]
            if not has_flag(self.compiler, "-std=c++17"):
                self.c_opts["unix"].remove("-std=c++17")
                self.c_opts["unix"].append("-std=c++1z")

        opts = self.c_opts.get(ct, [])
        if ct == "unix":
            opts.append('-DVERSION_INFO="%s"' % self.distribution.get_version())
            if has_flag(self.compiler, "-fvisibility=hidden"):
                opts.append("-fvisibility=hidden")
        elif ct == "msvc":
            opts.append('/DVERSION_INFO=\\"%s\\"' % self.distribution.get_version())

        if sys.platform == "win32":
            self.compiler.macros.append(("REPROCXX_SHARED", 1))
            self.compiler.macros.append(("GHC_WIN_DISABLE_WSTRING_STORAGE_TYPE", 1))
        self.compiler.macros.append(("MAMBA_ONLY", 1))

        for ext in self.extensions:
            ext.extra_compile_args = opts
        build_ext.build_extensions(self)


setup(
    name="mamba",
    version=__version__,
    author="Wolf Vollprecht",
    author_email="w.vollprecht@gmail.com",
    url="https://github.com/wolfv/mamba",
    description="A fast, libsolv based solver and installer for conda packages.",
    packages=["mamba"],
    entry_points={"console_scripts": ["mamba = mamba.mamba:main"]},
    long_description="A (hopefully faster) reimplementation of the slow bits of conda.",
    ext_modules=ext_modules,
    install_requires=[],
    extras_require={"test": ["pytest"]},
    cmdclass={"build_ext": BuildExt},
    zip_safe=False,
)
