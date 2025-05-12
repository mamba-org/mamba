.. _mamba:

Mamba User Guide
----------------

``mamba`` is a CLI tool to manage ``conda`` s environments.

If you already know ``conda``, great, you already know ``mamba``!

If you're new to this world, don't panic you will find everything you need in this documentation. We recommend to get familiar with :ref:`concepts<concepts>` first.


Quickstart
==========

The ``mamba create`` command creates a new environment.

You can create an environment with the name ``nameofmyenv`` by calling:

.. code:: shell

    mamba create -n nameofmyenv <list of packages>


After this process has finished, you can :ref:`activate<activation>` the virtual environment by
calling ``mamba activate <nameofmyenv>``.
For example, to install JupyterLab from the ``conda-forge`` channel and then run it, you could use
the following commands:

.. code:: shell

    mamba create -n myjlabenv jupyterlab -c conda-forge
    mamba activate myjlabenv  # activate our environment
    jupyter lab               # this will start up jupyter lab and open a browser

Once an environment is activated, ``mamba install`` can be used to install further packages
into the environment.

.. code:: shell

    mamba activate myjlabenv
    mamba install bqplot  # now you can use bqplot in myjlabenv
    mamba install "matplotlib>=3.5.0" cartopy  # now you installed matplotlib with version>=3.5.0 and default version of cartopy

Instead of activating an environment, you can also execute a command inside the environment by
calling ``mamba run -n <nameofmyenv>``.
For instance, the previous activation example is similar to:

.. code:: shell

    mamba run -n myjlabenv jupyter lab


``mamba`` vs ``conda`` CLIs
===========================

``mamba`` and ``conda`` mainly have the same command line arguments with a few differences.
For simple cases, you can swap one for the other.
For a full ``conda`` compatible experience, ``conda`` itself uses Mamba's solver under the hood,
so you can get great speedups from previous versions.

.. code::

  $ mamba --help

  shell                       Generate shell init scripts
  create                      Create new environment
  install                     Install packages in active environment
  update                      Update packages in active environment
  repoquery                   Find and analyze packages in active environment or channels
  remove, uninstall           Remove packages from active environment
  list                        List packages in active environment
  package                     Extract a package or bundle files into an archive
  clean                       Clean package cache
  config                      Configuration of micromamba
  info                        Information about micromamba
  constructor                 Commands to support using micromamba in constructor
  env                         See mamba/micromamba env --help
  activate                    Activate an environment
  run                         Run an executable in an environment
  ps                          Show, inspect or kill running processes
  auth                        Login or logout of a given host
  search                      Find packages in active environment or channels
                              This is equivalent to `repoquery search` command

Specification files
===================

The ``create`` syntax also allows you to use specification or environment files
(also called *spec files*) to easily re-create environments.

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

.. code:: shell

  mamba create -n from_file -f spec_file.txt -c conda-forge

.. note::

  You can pass multiple text spec files by repeating the ``-f,--file`` argument.


Conda YAML spec files
*********************

More powerful are ``YAML`` files like the following, because they already contain a desired environment name and the channels to use:

.. code:: yaml

  name: testenv
  channels:
    - conda-forge
  dependencies:
    - python >=3.6,<3.7
    - ipykernel >=5.1

    - ipywidgets[build_number=!=0]

They are used the same way as text files:

.. code:: shell

  mamba create -f env.yml

.. note::
  CLI options will keep :ref:`precedence<precedence-resolution>` over *target prefix* or *channels* specified in spec files.

.. note::
  You can pass multiple ``YAML`` spec files by repeating the ``-f,--file`` argument.

Explicit spec files
*******************

Using ``conda`` you can generate *explicit* environment lock files. For this, create an environment, activate it, and execute:

.. code:: shell

  conda list --explicit --md5

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

To install such a file with ``mamba``, just pass the ``-f`` flag again:

.. code:: shell

  mamba create -n xtensor -f explicit_env.txt

.. note::

   Explicit spec files are single-platform.

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

To install such a file with ``mamba``, just pass the ``-f`` flag again:

.. code::

  $ mamba create -n my-environment -f conda-lock.yml

Repoquery
=========

``mamba`` comes with features on top of stock ``conda``.
To efficiently query repositories and query package dependencies you can use ``mamba repoquery``.

Here are some examples:

.. code:: shell

    # will show you all available xtensor packages.
    mamba repoquery search xtensor

    # you can also specify more constraints on this search query
    mamba repoquery search "xtensor>=0.18"

    # will show you a list of the direct dependencies of xtensor.
    mamba repoquery depends xtensor

    # will show you a list of the dependencies (including dependencies of dependencies).
    mamba repoquery depends xtensor --recursive

The flag ``--recursive`` shows also recursive (i.e. transitive) dependencies of dependent packages instead of only direct dependencies.
With the ``-t,--tree`` flag, you can get the same information of a recursive query in a tree.

.. code::

    mamba repoquery depends -t xtensor

    xtensor == 0.21.5
    ├─ libgcc-ng [>=7.3.0]
    │ ├─ _libgcc_mutex [0.1 conda_forge]
    │ └─ _openmp_mutex [>=4.5]
    │   ├─ _libgcc_mutex already visited
    │   └─ libgomp [>=7.3.0]
    │     └─ _libgcc_mutex already visited
    ├─ libstdcxx-ng [>=7.3.0]
    └─ xtl [>=0.6.9,<0.7]
        ├─ libgcc-ng already visited
        └─ libstdcxx-ng already visited


And you can ask for the inverse, which packages depend on some other package (e.g. ``ipython``) using ``whoneeds``.

.. code::

    mamba repoquery whoneeds ipython

    Name            Version Build          Depends          Channel
    -------------------------------------------------------------------
    jupyter_console 6.4.3   pyhd3eb1b0_0   ipython          pkgs/main
    ipykernel       6.9.1   py39haa95532_0 ipython >=7.23.1 pkgs/main
    ipywidgets      7.6.5   pyhd3eb1b0_1   ipython >=4.0.0  pkgs/main


With the ``-t,--tree`` flag, you can get the same information in a tree.

.. code::

    mamba repoquery whoneeds -t ipython

    ipython[8.2.0]
    ├─ jupyter_console[6.4.3]
    │  └─ jupyter[1.0.0]
    ├─ ipykernel[6.9.1]
    │  ├─ notebook[6.4.8]
    │  │  ├─ widgetsnbextension[3.5.2]
    │  │  │  └─ ipywidgets[7.6.5]
    │  │  │     └─ jupyter already visited
    │  │  └─ jupyter already visited
    │  ├─ jupyter_console already visited
    │  ├─ ipywidgets already visited
    │  ├─ jupyter already visited
    │  └─ qtconsole[5.3.0]
    │     └─ jupyter already visited
    └─ ipywidgets already visited


.. note::

  ``depends`` and ``whoneeds`` sub-commands require either the specified package to be installed in you environment, or for the channel to be specified with the ``-c,--channel`` flag.
  When ``search`` sub-command is used without specifying the **channel** explicitly (using the flag previously mentioned), the search will be performed considering the channels set during the configuration.
