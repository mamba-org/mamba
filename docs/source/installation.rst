.. _installation:

============
Installation
============

.. _mamba-install:

mamba
=====

Fresh install
*************

We strongly recommend to start from ``mambaforge``, a community project of the conda-forge community.

| You can download `mambaforge <https://github.com/conda-forge/miniforge#mambaforge>`_ for Windows, macOS and Linux.
| ``mambaforge`` comes with the popular ``conda-forge`` channel preconfigured, but you can modify the configuration to use any channel you like.

 | After successful installation, you can use the mamba commands as described in :ref:`mamba user guide<mamba>`.


Existing ``conda`` install
**************************

If you are already a ``conda`` user, very good!

To get ``mamba``, just install it *into the base environment* from the ``conda-forge`` channel:

.. code::

   conda install mamba -n base -c conda-forge


.. warning::
   Installing mamba into any other environment than base can cause unexpected behavior

.. _umamba-install:

micromamba
==========

``micromamba`` is a tiny version of the ``mamba`` package manager.
It is a pure C++ package with a separate command line interface.
It is very tiny, does not need a ``base`` environment and does not come with a default version of Python.
It is completely statically linked, which allows you to drop it in some place and just execute it.
It can be used to bootstrap environments (as an alternative to miniconda).

Note: ``micromamba`` is currently experimental and it's advised to use it in containers & CI only.

.. _umamba-install-automatic-installation:

Automatic installation
**********************

If you are using macOS or Linux, there is a simple way of installing ``micromamba``.
Simply execute the installation script in your preferred shell.

For Linux, the default shell is ``bash``:

.. code:: bash

   curl micro.mamba.pm/install.sh | bash

For macOS, the default shell is ``zsh``:

.. code:: zsh

   curl micro.mamba.pm/install.sh | zsh

.. _umamba-install-manual-installation:

Manual installation
*******************

Download and unzip the executable (from the official conda-forge package):

.. _umamba-install-linux:

Linux
*****

Ensure that basic utilities are installed. We need ``wget`` and ``tar`` with support for ``bzip2``.
Also you need a glibc based system like Ubuntu, Fedora or Centos (Alpine Linux does not work natively).

The following magic URL always returns the latest available version of micromamba, and the ``bin/micromamba`` part is automatically extracted using ``tar``.

.. code:: sh

  wget -qO- https://micro.mamba.pm/api/micromamba/linux-64/latest | tar -xvj bin/micromamba


.. note::
  Additional builds are available for linux-aarch64 (ARM64) and linux-ppc64le. Just modify ``linux-64`` in the URL above to match the desired architecture.
  You can find the available builds on `anaconda.org <https://anaconda.org/conda-forge/micromamba/files>`_.

After extraction is completed, we can use the micromamba binary.

If you want to quickly use micromamba in an ad-hoc usecase, you can run

.. code::

  export MAMBA_ROOT_PREFIX=/some/prefix  # optional, defaults to ~/micromamba
  eval "$(./bin/micromamba shell hook -s posix)"

This shell hook modifies your shell variables to include the micromamba command.

If you want to persist these changes, you can automatically write them to your ``.bashrc`` (or ``.zshrc``) by running ``./micromamba shell init ...``.
This also allows you to choose a custom MAMBA_ROOT_ENVIRONMENT, which is where the packages and repodata cache will live.

.. code::

  sh
  ./bin/micromamba shell init -s bash -p ~/micromamba  # this writes to your .bashrc file
  # sourcing the bashrc file incorporates the changes into the running session.
  # better yet, restart your terminal!
  source ~/.bashrc

Now you can activate the base environment and install new packages, or create other environments.

.. code::

  micromamba activate  # this activates the base environment
  micromamba install python=3.6 jupyter -c conda-forge
  # or
  micromamba create -n env_name xtensor -c conda-forge
  micromamba activate env_name

.. _umamba-install-osx:

macOS
*****
``micromamba`` has macOS support as well.

brew
^^^^
If you have ``brew`` installed you can get ``micromamba`` as a cask with

.. code::

 brew install --cask micromamba


Manual
^^^^^^

Otherwise, you can use the download script; here, instructions are mostly the same as with :ref:`linux<umamba-install-linux>`.

However, you need to download a different `micromamba`` binary depending if you are using an Apple Silicon or an Intel Mac.need to download a different `micromamba` binary depending if you are using an Apple Silicon or an Intel Mac.

Apple Silicon:

.. code::

  curl -Ls https://micro.mamba.pm/api/micromamba/osx-arm64/latest | tar -xvj bin/micromamba
  mv bin/micromamba ./micromamba

Intel:

.. code:: bash

  curl -Ls https://micro.mamba.pm/api/micromamba/osx-64/latest | tar -xvj bin/micromamba
  mv bin/micromamba ./micromamba

The rest of the installation is the same for both Apple Silicon and Intel.

.. code::

  # directly execute the hook
  eval "$(./bin/micromamba shell hook -s posix)"

  # ... or shell init
  ./micromamba shell init -s zsh -p ~/micromamba
  source ~/.zshrc
  micromamba activate
  micromamba install python=3.6 jupyter -c conda-forge

.. _umamba-install-win:

Windows
*******

| ``micromamba`` also has Windows support! For Windows, we recommend powershell.
| Below are the commands to get micromamba installed in ``PowerShell``.


.. code:: powershell

  Invoke-Webrequest -URI https://micro.mamba.pm/api/micromamba/win-64/latest -OutFile micromamba.tar.bz2
  C:\PROGRA~1\7-Zip\7z.exe x micromamba.tar.bz2 -aoa
  C:\PROGRA~1\7-Zip\7z.exe x micromamba.tar -ttar -aoa -r Library\bin\micromamba.exe

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


.. _shell_completion:

Shell completion
================

For now, only ``micromamba`` provides shell completion on ``bash`` and ``zsh``.

To activate it, it's as simple as running:

.. code::

  micromamba shell completion

The completion is now available in any new shell opened or in the current shell after sourcing the configuration file to take modifications into account.

.. code::

  source ~/.<shell>rc

| Just hit ``<TAB><TAB>`` to get completion when typing your command.
| For example the following command will help you to pick a named environment to activate:

.. code::

  micromamba activate <TAB><TAB>


.. _umamba-install-api:

API
===

We should soon figure out an automated process to use the latest version of micromamba.
We can use the anaconda api: https://api.anaconda.org/release/conda-forge/micromamba/latest to find all the latest packages,
we just need to select the one for the right platform.
