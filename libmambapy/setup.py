import importlib.util
import os
import pathlib

import skbuild
import skbuild.constants

__dir__ = pathlib.Path(__file__).parent.absolute()


def CMAKE_INSTALL_DIR():
    """Where scikit-build configures CMAKE_INSTALL_PREFIX."""
    return os.path.abspath(skbuild.constants.CMAKE_INSTALL_DIR())


def libmambapy_version():
    """Get the version of libmambapy from its version module."""
    spec = importlib.util.spec_from_file_location(
        "libmambapy_version", __dir__ / "src/libmambapy/version.py"
    )
    ver = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ver)
    return ver.__version__


skbuild.setup(
    version=libmambapy_version(),
    packages=["libmambapy", "libmambapy.bindings"],
    package_dir={"": "src"},
    package_data={"libmambapy": ["py.typed", "__init__.pyi"]},
    cmake_languages=["CXX"],
    cmake_minimum_required_version="3.17",
    cmake_install_dir="src/libmambapy",  # Must match package_dir layout
    cmake_args=[
        f"-DMAMBA_INSTALL_PYTHON_EXT_LIBDIR={CMAKE_INSTALL_DIR()}/src/libmambapy",
        "-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON"
    ],
)
