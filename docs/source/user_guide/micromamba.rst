.. _micromamba:

=====================
Micromamba User Guide
=====================

``micromamba`` is a tiny version of the ``mamba`` package manager.
It is a statically linked C++ executable with a separate command line interface.
It does not need a ``base`` environment and does not come with a default version of Python.


Quickstarts
===========

| ``micromamba`` supports a subset of all ``mamba`` or ``conda`` commands and implements a command line interface from scratch.
| You can see all implemented commands with ``micromamba --help``:

.. code::

  $ micromamba --help

  Subcommands:
    shell                       Generate shell init scripts
    create                      Create new environment
    install                     Install packages in active environment
    update                      Update packages in active environment
    repoquery                   Find and analyze packages in active environment or channels
    remove                      Remove packages from active environment
    list                        List packages in active environment
    package                     Extract a package or bundle files into an archive
    clean                       Clean package cache
    config                      Configuration of micromamba
    info                        Information about micromamba
    constructor                 Commands to support using micromamba in constructor
    env                         See `mamba/micromamba env --help`
    activate                    Activate an environment
    run                         Run an executable in an environment
    ps                          Show, inspect or kill running processes
    auth                        Login or logout of a given host
    search                      Find packages in active environment or channels


To activate an environment just call ``micromamba activate /path/to/env`` or, when it's a named environment in your :ref:`root prefix<root-prefix>`, then you can also use ``micromamba activate myenv``.

``micromamba`` expects to find the *root prefix* set by ``$MAMBA_ROOT_PREFIX`` environment variable. You can also provide it using CLI option ``-r,--root-prefix``.

 | Named environments then live in ``$MAMBA_ROOT_PREFIX/envs/``.

For more details, please read about :ref:`configuration<configuration>`.

After :ref:`activation<activation>`, you can run ``install`` to add new packages to the environment.

.. code::

  $ micromamba install xtensor -c conda-forge


Using ``create``, you can also create environments:

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


After the installation is finished, the environment can be :ref:`activated<activation>` with:

.. code::

  $ micromamba activate xtensor_env


Specification files
===================

The ``create`` syntax also allows you to use specification or environment files (also called *spec files*) to easily re-create environments.

The supported syntaxes are:

.. contents:: :local:

Simple text spec files
**********************

The ``txt`` file contains *one spec per line*. For example, this could look like:

.. code::

  xtensor
  numpy 1.19
  xsimd >=7.4


To use this file, pass:

.. code::

  $ micromamba create -n from_file -f spec_file.txt -c conda-forge

.. note::
  You can pass multiple text spec files by repeating the ``-f,--file`` argument.


Conda YAML spec files
*********************

More powerful are ``YAML`` files like the following, because they already contain a desired environment name and the channels to use:

.. code::

  name: testenv
  channels:
    - conda-forge
  dependencies:
    - python >=3.6,<3.7
    - ipykernel >=5.1
    - ipywidgets[build_number=!=0]

They are used the same way as text files:

.. code::

  $ micromamba create -f env.yml

.. note::
  CLI options will keep :ref:`precedence<precedence-resolution>` over *target prefix* or *channels* specified in spec files.

.. note::
  You can pass multiple ``YAML`` spec files by repeating the ``-f,--file`` argument.

Explicit spec files
*******************

Using ``conda`` you can generate *explicit* environment lock files. For this, create an environment, activate it, and execute:

.. code::

  $ conda list --explicit --md5

These environment files look like the following and precisely "pin" the desired package + version + build string. Each package also has a checksum for reproducibility:

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

To install such a file with ``micromamba``, just pass the ``-f`` flag again:

.. code::

  $ micromamba create -n xtensor -f explicit_env.txt

.. note::

   Explicit spec files are single-platform.

.. _micromamba-conda-lock:

``conda-lock`` YAML spec files
******************************

Using ``conda-lock``, you can generate lockfiles which, like explicit spec files, pin precisely and include a checksum for each package for reproducibility.
Unlike explicit spec files, these "unified" lock files are multi-platform.

These files are named ``conda-lock.yml`` by default, and look like:

.. code::

    # This lock file was generated by conda-lock (https://github.com/conda/conda-lock). DO NOT EDIT!
    #
    # A "lock file" contains a concrete list of package versions (with checksums) to be installed. Unlike
    # e.g. `conda env create`, the resulting environment will not change as new package versions become
    # available, unless you explicitly update the lock file.
    #
    # Install this environment as "YOURENV" with:
    #     conda-lock install -n YOURENV --file conda-lock.yml
    # To update a single package to the latest version compatible with the version constraints in the source:
    #     conda-lock lock  --lockfile conda-lock.yml --update PACKAGE
    # To re-solve the entire environment, e.g. after changing a version constraint in the source file:
    #     conda-lock -f environment.yml --lockfile conda-lock.yml
    version: 1
    metadata:
      content_hash:
        osx-64: c2ccd3a86813af18ea19782a2f92b5a82e01c89f64a020ad6dea262aae638e48
        linux-64: 06e0621a9712fb0dc0b16270ddb3e0be16982b203fc71ffa07408bf4bb7c22ec
        win-64: efee77261626b3877b9d7cf7bf5bef09fd8e5ddfc79349a5f598ea6c8891ee84
      channels:
      - url: conda-forge
        used_env_vars: []
      platforms:
      - linux-64
      - osx-64
      - win-64
      sources:
      - environment.yml
    package:
    - name: _libgcc_mutex
      version: '0.1'
      manager: conda
      platform: linux-64
      dependencies: {}
      url: https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2
      hash:
        md5: d7c89558ba9fa0495403155b64376d81
        sha256: fe51de6107f9edc7aa4f786a70f4a883943bc9d39b3bb7307c04c41410990726
      category: main
      optional: false
    - name: ca-certificates
      version: 2023.5.7
      manager: conda
      platform: linux-64
      dependencies: {}
      url: https://conda.anaconda.org/conda-forge/linux-64/ca-certificates-2023.5.7-hbcca054_0.conda
      hash:
        md5: f5c65075fc34438d5b456c7f3f5ab695
        sha256: 0cf1bb3d0bfc5519b60af2c360fa4888fb838e1476b1e0f65b9dbc48b45c7345
      category: main
      optional: false

In order to YAML files to be considered as ``conda-lock`` files, their name must ends with ``-lock.yml`` or ``-lock.yaml``.

To install such a file with ``micromamba``, just pass the ``-f`` flag again:

.. code::

  $ micromamba create -n my-environment -f conda-lock.yml
