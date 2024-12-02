=======================
Development Environment
=======================

.. warning::

   These instructions have some inaccuracies on Windows.

Get the code and Mamba
======================

Clone the repo
**************

Fork and clone the repository in your preferred manner.
Refer to GitHub documentation for how to do so.

Install micromamba
******************

Setting the development environment requires conda, mamba, or micromamba.
Since some commands are automated with ``micromamba``, and to avoid any runtime linking issue,
it is best to work with micromamba.

Refer to the :ref:`installation<umamba-install>` page for how to install micromamba or mamba.

Running commands manually
=========================

.. note::

   The CI files in ``.github/workflow`` provide an alternative way of developing Mamba.

Install development dependencies
********************************

.. code:: bash

    micromamba create -n mamba -c conda-forge -f dev/environment-dev.yml
    micromamba activate mamba

Compile ``libmamba`` and other artifacts
****************************************

``libmamba`` is built using CMake.
Typically during development, we build everything dynamically using dependencies
from Conda-Forge.

The first step is to configure the build options.
A recommended set is already provided as CMake Preset, but feel free to use any variations.

.. note::
    If you do choose to use the provided CMake Preset, you may need to
    install ``ccache`` as an extra requirement as specified
    in ``dev/environment-dev-extra.yml``.

.. note::
    All ``cmake`` commands listed below use ``bash`` multi-line syntax.
    On Windows, replace ``\`` trailing character with ``^``.

.. code:: bash

    cmake -B build/ -G Ninja --preset mamba-unix-shared-debug-dev

Compilation can then be launched with:

.. code:: bash

    cmake --build build/ --parallel

``libmamba`` tests
******************

The tests for libmamba are written in C++.

.. code:: bash

    ./build/libmamba/tests/test_libmamba

``micromamba`` integration tests
********************************

Many ``micromamba`` integration tests are written through a pytest Python wrapper.
The environment variable ``TEST_MAMBA_EXE`` controls which executable is being tested.

.. code:: bash

   export TEST_MAMBA_EXE="${PWD}/build/micromamba/micromamba"
   python -m pytest micromamba/tests

``libmambapy`` tests
********************

To run the ``libmambapy`` tests, the Python package needs to be properly installed first.

.. warning::

   This needs to be done every time ``libmamba`` changes.

.. code:: bash

    cmake --install build/ --prefix "${CONDA_PREFIX}"

Then the Python bindings can be installed

.. code:: bash

    python -m pip install --no-deps --no-build-isolation libmambapy/

Finally the tests can be run:

.. code:: bash

    python -m pytest libmambapy/tests

Code Formatting
===============

Code formatting is done using Pre-Commit hooks.
Whichever way you decided to install development dependencies, we recommend installing
Pre-Commit hooks with

.. code:: bash

   pre-commit install

Alternatively, the checks can be run manually

.. code:: bash

   pre-commit run --all-files
