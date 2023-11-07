import os

import skbuild
import skbuild.constants


def CMAKE_INSTALL_DIR():
    """Where scikit-build configures CMAKE_INSTALL_PREFIX."""
    return os.path.abspath(skbuild.constants.CMAKE_INSTALL_DIR())


skbuild.setup(
    packages=["libmambapy", "libmambapy.core"],
    package_dir={"": "src/"},
    package_data={"libmambapy": ["py.typed", "__init__.pyi"]},
    cmake_languages=["CXX"],
    cmake_minimum_required_version="3.17",
    cmake_install_dir="src/libmambapy",  # Must match package_dir layout
    cmake_args=[
        f"-DMAMBA_INSTALL_PYTHON_EXT_LIBDIR={CMAKE_INSTALL_DIR()}/src/libmambapy",
    ],
)
