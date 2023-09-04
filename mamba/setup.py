# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

# -*- coding: utf-8 -*-

import os
import sys

from setuptools import setup

here = os.path.dirname(os.path.abspath(__file__))

version_ns = {}
with open(os.path.join(here, "mamba", "_version.py")) as f:
    exec(f.read(), {}, version_ns)

__version__ = version_ns["__version__"]

#data_files = [
    #("etc/profile.d", ["mamba/shell_templates/mamba.sh"]),
    #("etc/fish/conf.d", ["mamba/shell_templates/mamba.fish"]),
#]
#if sys.platform == "win32":
    #data_files.append(
        #("condabin", ["mamba/shell_templates/mamba.bat"]),
    #)
    #data_files.append(
        #("Library/bin", ["mamba/shell_templates/win_redirect/mamba.bat"]),
    #)

setup(
    name="mamba",
    version=__version__,
    author="Wolf Vollprecht",
    author_email="wolf.vollprecht@quantstack.net",
    url="https://github.com/mamba-org/mamba",
    description="A fast, libsolv based solver and installer for conda packages.",
    packages=["mamba"],
    entry_points={"console_scripts": ["mamba = mamba.mamba:main"]},
    long_description="A (hopefully faster) reimplementation of the slow bits of conda.",
    install_requires=[
        "libmambapy",
    ],
    extras_require={"test": ["pytest", "pytest-lazy-fixture"]},
    #data_files=data_files,
    include_package_data=True,
    zip_safe=False,
)
