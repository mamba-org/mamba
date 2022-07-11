=============
Build locally
=============

Local development environment
=============================

Clone the repo
**************

Start by cloning the repo:

.. code::

    git clone https://github.com/mamba-org/mamba.git

Then move to the newly created directory:

.. code::

    cd mamba

Install dev requirements
************************

| Make sure you have Mamba's development requirements installed in your environment.
| To do so, you can use your existing ``mamba`` or ``micromamba`` installation:

.. code::

    mamba env update --name <env_name> --file ./<project>/environment-dev.yml

.. code::

    micromamba install --name <env_name> --file ./<project>/environment-dev.yml

Pick the correction environment spec file depending on the project you want to build.

If you don't have an existing env, refer to the :ref:`installation<installation>` page.

The different parts of this repository are sub-projects, all relying (totally or partially) on ``cmake`` configuration.

.. note::
    All ``cmake`` commands listed below use ``bash`` multi-line syntax.
    On Windows, replace ``\`` trailing character with ``^``.

.. note::
    Feel free to use your favorite generator: ``make``, ``ninja``, etc.


Build ``libmamba``
==================

``libmamba`` build is enabled using ``BUILD_LIBMAMBA`` ``cmake`` option.

Shared library
**************

``BUILD_SHARED`` option needs to be activated:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_SHARED=ON
    make

.. note::
    ``libmamba`` ``cmake`` target represents the shared library

Static library
**************

``BUILD_STATIC`` option needs to be activated:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_STATIC=ON
    make

.. note::
    ``libmamba-static`` ``cmake`` target represents the static library without any dependency linkage

Fully static library
********************

``BUILD_STATIC_DEPS`` option needs to be activated:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_STATIC_DEPS=ON
    make

.. note::
    ``libmamba-full-static`` ``cmake`` target represents the static library with static dependencies linkage

.. note::
    The ``libmamba`` static library does not embed the dependencies but the ``cmake`` target will expose those dependencies to any executable linking on it

.. note::
    The fully statically lib still has few symbols required from system shared libraries (``glibc`` for instance)

.. warning::
    This version of the library has a small difference versus the static and shared ones, on the way the SSL backend of cURL is set
    See `libmamba/src/core/fetch.cpp` for more information


Tests
*****

| C++ tests require ``libmamba`` to be built.
| To compile the ``gtest``-based C++ test suite, run:

.. code::

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_SHARED=ON \
        -DBUILD_LIBMAMBA_TESTS=ON
    make

You should now be able to run:

.. code::

    ./libmamba/tests/test_libmamba

Alternatively you can use:

.. code::

    make test

Build ``libmambapy``
====================

The Python bindings of ``libmamba``, ``libmambapy`` can be built by using the ``BUILD_LIBMAMBAPY`` ``cmake`` option.

You can either rely on ``libmamba`` package already installed in your environment and run:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBAPY=ON
    make

or rebuild ``libmamba`` in the same time:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_SHARED=ON \
        -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX \
        -DBUILD_LIBMAMBAPY=ON
    make

.. note::
    When rebuilding ``libmamba``, you also need to install the library in a path it will be found.
    Use for that the ``CMAKE_INSTALL_PREFIX`` ``cmake`` option to point your current development environment:

    - ``-DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX`` [unix]
    - ``-DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX\\Library`` [win]

You'll need to install the target to have the bindings Python sub-module correctly located, then you can use ``pip`` to install that Python package:

.. code:: bash

    make install
    pip install -e ../libmambapy/ --no-deps

.. note::
    The editable mode ``-e`` provided by ``pip`` allows to use the source directory as the Python package instead of copying sources inside the environment
    You can then change the code without having to reinstall the package

.. note::
    The ``--no-deps`` tells ``pip`` to skip the dependencies installation, since they are already installed (``libmamba`` installed using ``cmake``)

.. note::
    ``libmambapy`` is dynamically linking against ``libmamba`` (shared) library


Build ``mamba``
===============

You need to build and install ``libmambapy``, which is a dependency of ``mamba``, then install ``mamba``:

.. code::
    pip install -e ./mamba/ --no-deps

.. note::
    Other dependencies are already installed from the `environment-dev.yml` file


Build ``micromamba``
====================

Dynamically linked
******************

To build ``micromamba``, activate the ``BUILD_MICROMAMBA`` flag in ``cmake`` command:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_MICROMAMBA=ON \
        -DMICROMAMBA_LINKAGE=DYNAMIC
    make

or rebuild ``libmamba`` in the same time:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_SHARED=ON \
        -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX \
        -DBUILD_MICROMAMBA=ON \
        -DMICROMAMBA_LINKAGE=DYNAMIC
    make

.. note::
    If you need to install, use the ``CMAKE_INSTALL_PREFIX`` ``cmake`` option to point your current development environment:

    - ``-DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX`` [unix]
    - ``-DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX\\Library`` [win]

.. note::
    ``micromamba`` will be dynamically linked against ``libmamba`` and all its dependencies (``libsolv``, ``libarchive``, etc.)

.. note::
    ``MICROMAMBA_LINKAGE`` default value is ``DYNAMIC``

Statically linked
*****************

You can also build ``micromamba`` statically linked against ``libmamba``:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_MICROMAMBA=ON \
        -DMICROMAMBA_LINKAGE=STATIC
    make

or with ``libmamba``:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_STATIC=ON \
        -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX \
        -DBUILD_MICROMAMBA=ON \
        -DMICROMAMBA_LINKAGE=STATIC
    make

.. note::
    ``MICROMAMBA_LINKAGE`` default value is ``DYNAMIC``

.. note::
    ``micromamba`` will be then statically linked against ``libmamba`` but still dynamically linked against all its dependencies (``libsolv``, ``libarchive``, etc.)

Fully statically linked
***********************

``micromamba`` can be built as a fully statically linked executable. For that, you need to install extra requirements:

.. code::

    micromamba install --name <env_name> --file ./micromamba/environment-static-dev.yml

It will install the development version of dependencies, including static libraries.

Now you can run ``cmake`` with the additional flag ``MICROMAMBA_STATIC_DEPS`` turned on:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_MICROMAMBA=ON \
        -DMICROMAMBA_LINKAGE=FULL_STATIC
    make

or with ``libmamba``:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_STATIC_DEPS=ON \
        -DBUILD_MICROMAMBA=ON \
        -DMICROMAMBA_LINKAGE=FULL_STATIC
    make

Tests
*****

You should be able to run the Python-based test suite:

.. code::

    pytest ./micromamba/tests/
