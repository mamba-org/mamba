
.. _umamba-install:

=======================
Micromamba Installation
=======================

``micromamba`` is a fully statically-linked, self-contained, executable.
This means that the ``base`` environment is completely empty.
The configuration for ``micromamba`` is slighly different, namely all environments and cache will be
created by default under the ``MAMBA_ROOT_PREFIX`` environment variable.
There is also no pre-configured ``.condarc``/``.mambarc`` shipped with micromamba
(they are however still read if present).

.. _umamba-install-automatic-installation:

Operating System package managers
*********************************
Homebrew
^^^^^^^^

On macOS, you can install ``micromamba`` from `Homebrew <https://brew.sh/>`_:

.. code:: bash

   brew install micromamba


Mamba-org releases
******************
Automatic install
^^^^^^^^^^^^^^^^^

If you are using macOS, Linux, or Git Bash on Windows there is a simple way of installing ``micromamba``.
Simply execute the installation script in your preferred shell.

For Linux, macOS, or Git Bash on Windows install with:

.. We use ``bash <(curl ...)`` instead of ``curl .. | bash`` as the latter does not work with prompts

.. code:: bash

   "${SHELL}" <(curl -L micro.mamba.pm/install.sh)

On Windows Powershell, use

.. code :: powershell

   Invoke-Expression ((Invoke-WebRequest -Uri https://micro.mamba.pm/install.ps1).Content)

.. note::

   The installation instructions executed by this script are available on
   `mamba-org/micromamba-releases <https://github.com/mamba-org/micromamba-releases>`_.
   Feel free to have a look and remix the steps as you see fit for a custom installation.

Self updates
^^^^^^^^^^^^
Once installed, ``micromamba`` can be updated with

.. code-block:: bash

   micromamba self-update

A explicit version can be specified with

.. code-block:: bash

   micromamba self-update --version 1.4.6


Nightly builds
**************

You can download fully statically linked builds for each commit to ``main`` on GitHub
(scroll to the bottom of the "Summary" page):
https://github.com/mamba-org/mamba/actions/workflows/static_build.yml?query=is%3Asuccess

Docker images
*************

The `mambaorg/micromamba <https://hub.docker.com/r/mambaorg/micromamba>`_ docker
image can be used to run ``micromamba`` without installing it:

.. code-block:: bash

  docker run -it --rm mambaorg/micromamba:latest micromamba info


Build from source
*****************

.. note::

   These instuction do not work currently on Windows, which requires a more complex hybrid build.
   For up-to-date instructions on Windows and Unix, consult the scripts in the
   `micromamba-feedstock <https://github.com/conda-forge/micromamba-feedstock>`_.

To build from source, install the development dependencies, using a Conda compatible installer
(``conda``/``mamba``/``micromamba``/``rattler``/``pixi``).

.. code-block:: bash

  micromamba create -n mamba --file dev/environment-micromamba-static.yml
  micromamba activate -n mamba

Use CMake from this environment to drive the build:

.. code-block:: bash

   cmake -B build/ \
       -G Ninja \
       ${CMAKE_ARGS} \
       -D CMAKE_BUILD_TYPE="Release" \
       -D BUILD_LIBMAMBA=ON \
       -D BUILD_STATIC=ON \
       -D BUILD_MICROMAMBA=ON
   cmake --build build/ --parallel

You will find the executable under "build/micromamba/micromamba".
The executable can be striped to remove its size:

.. code:: bash

   strip "build/micromamba/micromamba"


.. _shell_completion:

Shell completion
****************

For now, only ``micromamba`` provides shell completion on ``bash`` and ``zsh``.

To activate it, it's as simple as running:

.. code:: bash

  micromamba shell completion

The completion is now available in any new shell opened or in the current shell after sourcing the configuration file to take modifications into account.

.. code-block:: sh

  source ~/.<shell>rc

| Just hit ``<TAB><TAB>`` to get completion when typing your command.
| For example the following command will help you to pick a named environment to activate:

.. code-block:: bash

  micromamba activate <TAB><TAB>
