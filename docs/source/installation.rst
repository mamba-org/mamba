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

``micromamba`` is completely statically linked, which allows you to drop it in some place and just execute it.

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
  micromamba create -n new_prefix xtensor -c conda-forge
  micromamba activate new_prefix

.. _umamba-install-osx:

OS X
****

``micromamba`` has OS X support as well. Instructions are largely the same as :ref:`linux<umamba-install-linux>`:

.. code:: bash

  curl -Ls https://micro.mamba.pm/api/micromamba/osx-64/latest | tar -xvj bin/micromamba
  mv bin/micromamba ./micromamba

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
