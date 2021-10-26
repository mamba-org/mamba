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

Also needs ``BUILD_SHARED`` to be activated:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_SHARED=ON
    make

Static library
**************

Also needs ``BUILD_STATIC`` to be activated:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA=ON \
        -DBUILD_STATIC=ON
    make

Tests
*****

First, compile the ``gtest``-based C++ test suite:

.. code::

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBA_TESTS=ON
    make

You should now be able to run:

.. code::

    ./libmamba/tests/test_libmamba

Alternatively you can use:

.. code::

    make test

.. note::
    The static version of ``libmamba`` is necessary to build the C++ tests, it will be automatically turned-on


Build ``libmambapy``
====================

The Python bindings of ``libmamba``, ``libmambapy`` can be built by using the ``BUILD_LIBMAMBAPY`` ``cmake`` option.

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -DBUILD_LIBMAMBAPY=ON
    make

You'll need to install the target to have the bindings Python sub-module correctly located, then you can use ``pip`` to install that Python package:

.. code:: bash

    make install
    pip install -e ../libmambapy/ --no-deps

.. note::
    The editable mode ``-e`` provided by ``pip`` allows to use the source directory as the Python package instead of copying sources inside the environment
    You can then change the code without having to reinstall the package

.. note::
    The ``--no-deps`` tells ``pip`` to skip the dependencies installation, since they are already installed (``libmamba`` installed using ``cmake``)


Build ``mamba``
===============

You need to build and install ``libmambapy``, which is a dependency of ``mamba``, then install ``mamba``:

.. code::
    pip install -e ./mamba/ --no-deps


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
    make

.. note::
    If you need to install the executable, use your current development environment:

    - ``-DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX`` [unix]
    - ``-DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX\\Library`` [win]

| Doing so, you have built the dynamically linked version of ``micromamba``.
| It's well fitted for development but is not the artifact shipped when installing ``micromamba`` because it still requires the dependencies shared libraries to work.


Statically linked
*****************

``micromamba`` is a fully statically linked executable. To build it, you need to install extra requirements:

.. code::

    micromamba install --name <env_name> --file ./micromamba/environment-static-dev.yml

It will install the development version of dependencies, including static libraries.

Now you can run ``cmake`` with the additional flag ``STATICALLY_LINK_DEPS`` turned on:

.. code:: bash

    mkdir -p build
    cd build
    cmake .. \
        -BUILD_MICROMAMBA=ON \
        -DSTATICALLY_LINK_DEPS=ON
    make

Tests
*****

You should be able to run the Python-based test suite:

.. code::

    pytest ./micromamba/tests/
