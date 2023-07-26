.. _installation:
============
Installation
============

.. _mamba-install:
``mamba`` distribution
======================

Fresh install
*************

For a pre-configured installation, minimally different from ``conda``, the Mambaforge distribution
is a community project of the conda-forge community that packages ``mamba``.

| You can download `Mambaforge <https://github.com/conda-forge/miniforge#mambaforge>`_ for Windows, macOS and Linux.
| Mambaforge comes with the popular ``conda-forge`` channel preconfigured, but you can modify the configuration to use any channel you like.

 | After successful installation, you can use the mamba commands as described in :ref:`mamba user guide<mamba>`.

.. note::
   For both ``mamba`` and ``conda``, the ``base`` environment is meant to hold their dependencies.
   It is strongly discouraged to install anything else in the base envionment.
   Doing so may break ``mamba`` and ``conda`` installation.


Existing ``conda`` install (not recommended)
********************************************

.. warning::
   This way of installing Mamba is **not recommended**.
   We strongly recommend to use the Mambaforge method (see above).

To get ``mamba``, just install it *into the base environment* from the ``conda-forge`` channel:

.. code:: bash

   # NOT RECOMMENDED: This method of installation is not recommended, prefer Mambaforge instead (see above)
   # conda install -n base --override-channels -c conda-forge mamba 'python_abi=*=*cp*'


.. warning::
   Installing mamba into any other environment than ``base`` is not supported.

.. _umamba-install:
``micromamba`` standalone executable
====================================

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
Install script
^^^^^^^^^^^^^^

If you are using macOS or Linux, there is a simple way of installing ``micromamba``.
Simply execute the installation script in your preferred shell.

For Linux and macOS install with:

.. We use ``bash <(curl ...)`` instead of ``curl .. | bash`` as the latter does not work with prompts

.. code:: bash

   "${SHELL}" <(curl -L micro.mamba.pm/install.sh)

Self updates
^^^^^^^^^^^^
Once installed, ``micromamba`` can be updated with

.. code-block:: bash

   micromamba self-update

A explicit version can be specified with

.. code-block:: bash

   micromamba self-update --version 1.4.6

.. _umamba-install-manual-installation:
Manual installation
^^^^^^^^^^^^^^^^^^^
.. _umamba-install-posix:
Linux and macOS
~~~~~~~~~~~~~~~

Download and unzip the executable (from the official conda-forge package):

Ensure that basic utilities are installed. We need ``curl`` and ``tar`` with support for ``bzip2``.
Also you need a glibc based system like Ubuntu, Fedora or Centos (Alpine Linux does not work natively).

The following magic URL always returns the latest available version of micromamba, and the ``bin/micromamba`` part is automatically extracted using ``tar``.

.. code:: bash

  # Linux Intel (x86_64):
  curl -Ls https://micro.mamba.pm/api/micromamba/linux-64/latest | tar -xvj bin/micromamba
  # Linux ARM64:
  curl -Ls https://micro.mamba.pm/api/micromamba/linux-aarch64/latest | tar -xvj bin/micromamba
  # Linux Power:
  curl -Ls https://micro.mamba.pm/api/micromamba/linux-ppc64le/latest | tar -xvj bin/micromamba
  # macOS Intel (x86_64):
  curl -Ls https://micro.mamba.pm/api/micromamba/osx-64/latest | tar -xvj bin/micromamba
  # macOS Silicon/M1 (ARM64):
  curl -Ls https://micro.mamba.pm/api/micromamba/osx-arm64/latest | tar -xvj bin/micromamba

After extraction is completed, we can use the micromamba binary.

If you want to quickly use micromamba in an ad-hoc usecase, you can run

.. code:: bash

  export MAMBA_ROOT_PREFIX=/some/prefix  # optional, defaults to ~/micromamba
  eval "$(./bin/micromamba shell hook -s posix)"

This shell hook modifies your shell variables to include the micromamba command.

If you want to persist these changes, you can automatically write them to your ``.bashrc`` (or ``.zshrc``) by running ``./micromamba shell init ...``.
This also allows you to choose a custom MAMBA_ROOT_ENVIRONMENT, which is where the packages and repodata cache will live.

.. code:: sh

  # Linux/bash:
  ./bin/micromamba shell init -s bash -p ~/micromamba  # this writes to your .bashrc file
  # sourcing the bashrc file incorporates the changes into the running session.
  # better yet, restart your terminal!
  source ~/.bashrc

  # macOS/zsh:
  ./micromamba shell init -s zsh -p ~/micromamba
  source ~/.zshrc

Now you can activate the base environment and install new packages, or create other environments.

.. code:: bash

  micromamba activate  # this activates the base environment
  micromamba install python=3.6 jupyter -c conda-forge
  # or
  micromamba create -n env_name xtensor -c conda-forge
  micromamba activate env_name

An exclusive `conda-forge <https://conda-forge.org/>`_ setup can be configured with:

.. code-block:: bash

   micromamba config append channels conda-forge
   micromamba config append channels nodefaults
   micromamba set channel_priority strict

.. _umamba-install-win:
Windows
~~~~~~~

| ``micromamba`` also has Windows support! For Windows, we recommend powershell.
| Below are the commands to get micromamba installed in ``PowerShell``.

.. code-block:: powershell

  Invoke-Webrequest -URI https://micro.mamba.pm/api/micromamba/win-64/latest -OutFile micromamba.tar.bz2
  tar xf micromamba.tar.bz2

  MOVE -Force Library\bin\micromamba.exe micromamba.exe
  .\micromamba.exe --help

  # You can use e.g. $HOME\micromambaenv as your base prefix
  $Env:MAMBA_ROOT_PREFIX="C:\Your\Root\Prefix"

  # Invoke the hook
  .\micromamba.exe shell hook -s powershell | Out-String | Invoke-Expression

  # ... or initialize the shell
  .\micromamba.exe shell init -s powershell -p C:\Your\Root\Prefix
  # and use micromamba directly
  micromamba create -f ./test/env_win.yaml -y
  micromamba activate yourenv

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

.. _shell_completion:

Shell completion
================

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
