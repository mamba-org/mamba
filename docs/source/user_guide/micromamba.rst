.. _micromamba:

==========
Micromamba
==========

Micromamba is a small, pure-C++ executable with enough functionalities to *bootstrap* fully functional conda-environments.
Currently, given micromamba's early stage, it's main usage is in continous integration pipelines: since it's a single executable, based on mamba, it bootstraps *fast*.

.. warning::
  Note that micromamba is still in early development and not considered ready-for-general-usage yet! It has been used with great success in CI systems, though, and we encourage this usage.


## Installation

``micromamba`` works in the bash & zsh shell on Linux & OS X.
It's completely statically linked, which allows you to drop it in some place and just execute it.

Note: it's advised to use micromamba in containers & CI only.

Download and unzip the executable (from the official conda-forge package):

### Linux / Basic Usage

Ensure that basic utilities are installed. We need ``wget`` and ``tar`` with support for ``bzip2``.
Also you need a glibc based system like Ubuntu, Fedora or Centos (Alpine Linux does not work natively).

The following magic URL always returns the latest available version of micromamba, and the ``bin/micromamba`` part is automatically extracted using ``tar``.

.. code::

  sh
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

  sh
  micromamba activate  # this activates the base environment
  micromamba install python=3.6 jupyter -c conda-forge
  # or
  micromamba create -n new_prefix xtensor -c conda-forge
  micromamba activate new_prefix


### OS X

Micromamba has OS X support as well. Instructions are largely the same:

.. code::

  sh
  curl -Ls https://micro.mamba.pm/api/micromamba/osx-64/latest | tar -xvj bin/micromamba
  mv bin/micromamba ./micromamba

  # directly execute the hook
  eval "$(./bin/micromamba shell hook -s posix)"

  # ... or shell init
  ./micromamba shell init -s zsh -p ~/micromamba
  source ~/.zshrc
  micromamba activate
  micromamba install python=3.6 jupyter -c conda-forge

### Windows

Micromamba also has Windows support! For Windows, we recommend powershell. Below are the commands to get micromamba installed in ``PowerShell``.

.. code::

  powershell
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
  micromamba activate ...

## Available commands

Micromamba supports a subset of all conda commands and implements a command line interface from scratch. You can see all implemented commands with ``micromamba --help``:

.. code::

  $ micromamba --help

  Subcommands:
    activate                    Activate a conda / micromamba environment
    deactivate                  Deactivate a conda / micromamba environment
    shell                       Generate shell init scripts
    create                      Create new environment
    install                     Install packages in active environment
    remove                      Remove packages from active environment
    list                        List packages in active environment
    constructor                 Commands to support using micromamba in constructor

To activate an environment just call ``micromamba activate /path/to/env`` or, when it's a named environment in your ``$MAMBA_ROOT_PREFIX``, then you can just use ``micromamba activate myenv``. Named environments live in ``$MAMBA_ROOT_PREFIX/envs/``.

After activation, the ``$CONDA_PREFIX`` and ``$PATH`` environment variables are modified to add the environment to your current bash session. You can then run install to add new packages to the environment.

.. code::

  $ micromamba install xtensor -c conda-forge


Using create you can also create new virtual environments. Named environments are convenient and easy to use. For example, to create a new environment:

.. code::

  $ micromamba create -n xtensor_env xtensor xsimd -c conda-forge
                                             __
            __  ______ ___  ____ _____ ___  / /_  ____ _
           / / / / __ `__ \/ __ `/ __ `__ \/ __ \/ __ `/
          / /_/ / / / / / / /_/ / / / / / / /_/ / /_/ /
         / .___/_/ /_/ /_/\__,_/_/ /_/ /_/_.___/\__,_/
        /_/

  conda-forge/noarch       [====================] (00m:01s) Done
  conda-forge/linux-64     [====================] (00m:04s) Done

  Transaction

    Prefix: /home/wolfv/miniconda3/envs/xtensor_env

    Updating specs:

    - xtensor
    - xsimd


    Package        Version  Build        Channel                    Size
  ────────────────────────────────────────────────────────────────────────
    Install:
  ────────────────────────────────────────────────────────────────────────

    _libgcc_mutex      0.1  conda_forge  conda-forge/linux-64     Cached
    _openmp_mutex      4.5  1_gnu        conda-forge/linux-64     Cached
    libgcc-ng        9.3.0  h5dbcf3e_17  conda-forge/linux-64     Cached
    libgomp          9.3.0  h5dbcf3e_17  conda-forge/linux-64     Cached
    libstdcxx-ng     9.3.0  h2ae2ef3_17  conda-forge/linux-64     Cached
    xsimd            7.4.9  hc9558a2_0   conda-forge/linux-64     102 KB
    xtensor         0.21.9  h0efe328_0   conda-forge/linux-64     183 KB
    xtl             0.6.21  h0efe328_0   conda-forge/linux-64     Cached

    Summary:

    Install: 8 packages

    Total download: 285 KB

  ────────────────────────────────────────────────────────────────────────

  Confirm changes: [Y/n] ...


After the installation is finished, the environment can be activated with

.. code::

  $ micromamba activate xtensor_env


## Environment files

The create syntax also allows you to use spec- or environment files to easily recreate environments.

The three supported syntaxes are: yaml, txt and explicit spec files.

### Simple text spec files

The txt file contains *one spec per line*. For example, this could look like:

.. code::

  xtensor
  numpy 1.19
  xsimd >=7.4


To use this file, pass

.. code::

  $ micromamba create -n from_file -f spec_file.txt -c conda-forge


Note: with spec text files you can pass multiple files by repeating the ``-f`` argument.

### YAML environment files

More powerful are YAML files like the following, because they already contain a desired environment name and the channels to use.

.. code::

  name: testenv
  channels:
    - conda-forge
  dependencies:
    - python >=3.6,<3.7
    - ipykernel >=5.1
    - ipywidgets

.. code::

  $ micromamba create -f env.yml

YAML files do not allow multiple files.

### Explicit environment specs

Using conda you can generate *explicit* environment lock files. For this, create an environment and execute

.. code::

  $ conda list --explicit --md5

These environment files look like the following and precisely "pin" the desired package + version + build string.

.. code::

  # This file may be used to create an environment using:
  # $ conda create --name <env> --file <this file>
  # platform: linux-64
  @EXPLICIT
  https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2#d7c89558ba9fa0495403155b64376d81
  https://conda.anaconda.org/conda-forge/linux-64/libstdcxx-ng-9.3.0-h2ae2ef3_17.tar.bz2#342f3c931d0a3a209ab09a522469d20c
  https://conda.anaconda.org/conda-forge/linux-64/libgomp-9.3.0-h5dbcf3e_17.tar.bz2#8fd587013b9da8b52050268d50c12305
  https://conda.anaconda.org/conda-forge/linux-64/_openmp_mutex-4.5-1_gnu.tar.bz2#561e277319a41d4f24f5c05a9ef63c04
  https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-9.3.0-h5dbcf3e_17.tar.bz2#fc9f5adabc4d55cd4b491332adc413e0
  https://conda.anaconda.org/conda-forge/linux-64/xtl-0.6.21-h0efe328_0.tar.bz2#9eee90b98fd394db7a049792e67e1659
  https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.8-hc9558a2_0.tar.bz2#1030174db5c183f3afb4181a0a02873d

To install such a file with micromamba, just pass the ``-f`` flag again:

.. code::

  $ micromamba create -n xtensor -c conda-forge -f explicit_env.txt
