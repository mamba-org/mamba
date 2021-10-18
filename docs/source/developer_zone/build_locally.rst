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

    mamba env update --name <env_name> --file environment-dev.yml

.. code::

    micromamba install --name <env_name> --file environment-dev.yml

If you don't have one, refer to the :ref:`installation<installation>` page.

``mamba`` relies on ``python setup.py`` installation while others targets rely on ``cmake`` configuration.

.. note::
    All ``cmake`` commands listed below use ``bash`` multi-line syntax.
    On Windows, replace ``\`` trailing character with ``^``.



Build ``mamba``
===============

If you build ``mamba`` in a different environment than *base*, you must also install ``conda``
in that environment:

.. code::

    mamba install conda -c conda-forge

For a local (dev) build, run in the environment:

.. code::

    pip install -e .

This will build and install ``mamba`` in the current environment.

Build ``micromamba``
====================

Dynamically linked
******************

To build ``micromamba``, just activate the ``BUILD_MICROMAMBA`` flag in ``cmake`` command:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -BUILD_MICROMAMBA=ON
    cmake --build . -j

.. note::
    Feel free to use your favorite generator: ``make``, ``ninja``, etc.

.. note::
    If you need to install, use to install in your current development environment:

    - ``-DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX`` [unix]
    - ``-DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX\\Library`` [win]

| Doing so, you have built the dynamically linked version of ``micromamba``.
| It's well fitted for development but is not the artifact shipped when installing ``micromamba``.


Statically linked
*****************

``micromamba`` is a fully statically linked executable. To build it, you need to install extra requirements:

.. code::

    micromamba install --name <env_name> --file environment-static-dev.yml

It will install the development version of dependencies, including static libraries.

Now you can run ``cmake`` with the additional flag ``STATIC_DEPENDENCIES`` turned on:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DSTATIC_DEPENDENCIES=ON
    cmake --build . -j

Tests
*****

You should now be able to run the Python-based test suite:

.. code::

    pytest ./test/micromamba


Build ``libmamba``
==================

Shared library
**************

You need to enable the build of ``libmamba`` shared library using ``BUILD_SHARED`` flag in ``cmake`` command:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_SHARED=ON
    cmake --build . -j

Static library
**************

| The static build of ``libmamba`` is enabled by default (``BUILD_STATIC=ON``).
| You can run :

.. code:: bash

    mkdir -p build
    cd build
    cmake ..
    cmake --build . -j

Tests
*****

First, compile the ``gtest``-based C++ test suite:

.. code::

    mkdir -p build
    cd build
    cmake .. \
        -DENABLE_TESTS=ON
    cmake --build . -j

You should now be able to run:

.. code::

    ./test/test_mamba

Alternatively you can use:

.. code::

    make test

.. note::
    The static version of ``libmamba`` is necessary to build the C++ tests, don't disable it!
