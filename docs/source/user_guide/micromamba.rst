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

.. code-block::

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
    env                         List environments
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

.. code-block::

  micromamba install xtensor -c conda-forge


Using ``create``, you can also create environments:

.. code-block::

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

.. code-block::

  micromamba activate xtensor_env


Specification files
===================

The ``create`` syntax also allows you to use specification or environment files (also called *spec files*) to easily re-create environments.

The supported syntaxes are:

.. contents:: :local:

Simple text spec files
**********************

The ``txt`` file contains *one spec per line*. For example, this could look like:

.. code-block::

  xtensor
  numpy 1.19
  xsimd >=7.4


To use this file, pass:

.. code-block::

  micromamba create -n from_file -f spec_file.txt -c conda-forge

.. note::
  You can pass multiple text spec files by repeating the ``-f,--file`` argument.


Conda YAML spec files
*********************

More powerful are ``YAML`` files like the following, because they already contain a desired environment name and the channels to use:

.. code-block:: yaml

  name: testenv
  channels:
    - conda-forge
  dependencies:
    - python >=3.6,<3.7
    - ipykernel >=5.1
    - ipywidgets

They are used the same way as text files:

.. code-block::

  micromamba create -f env.yml

.. note::
  CLI options will keep :ref:`precedence<precedence-resolution>` over *target prefix* or *channels* specified in spec files.

.. note::
  You can pass multiple ``YAML`` spec files by repeating the ``-f,--file`` argument.

Explicit spec files
*******************

Using ``conda`` you can generate *explicit* environment lock files. For this, create an environment, activate it, and execute:

.. code-block::

  conda list --explicit --md5

These environment files look like the following and precisely "pin" the desired package + version + build string. Each package also has a checksum for reproducibility:

.. code-block::

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

.. code-block::

  micromamba create -n xtensor -f explicit_env.txt

.. note::

   Explicit spec files are single-platform.

.. _micromamba-conda-lock:

``conda-lock`` YAML spec files
******************************

Using ``conda-lock``, you can generate lockfiles which, like explicit spec files, pin precisely and include a checksum for each package for reproducibility.
Unlike explicit spec files, these "unified" lock files are multi-platform.

These files are named ``conda-lock.yml`` by default, and look like:

.. code-block:: yaml

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

To install such a file with ``micromamba``, just pass the ``-f`` flag again:

.. code-block::

  micromamba create -n my-environment -f conda-lock.yml

.. note::

  The lock file must end with ``-lock.yml`` for ``micromamba`` to recognize it
  as such.

Offline Usage
=============

Sometimes it is necessary to set up a Python environment on a machine that
cannot be connected to the internet. ``micromamba`` is a convenient solution for
this scenario since it is a statically linked binary, so no "installation" is
required, and no admin privileges are required [#f1]_. To successfully create an
environment, the offline machine needs a list of packages to be installed and
the packages themselves. You may be able to use one of the `specification file
<#specification-files>`_ formats mentioned above to satisfy the former
requirement, but this requires that the environment be re-solved on the offline
machine, which is highly likely to fail [#f2]_. Using the ``conda-lock``
strategy discussed `above <#micromamba-conda-lock>`_, we can solve the
environment on the online machine and create an explicit, comprehensive
dependency list for multiple platforms. The explicit list of packages removes
the need to solve for the environment on the offline machine. We can then use
``micromamba`` to download the list of packages in an isolated root prefix. The
steps below outline the process for moving an environment to an offline machine
without any installations or additional packages required on the offline
machine.

.. [#f1] While long path support is not strictly required for Windows machines, it
   is recommended to be enabled for machines that do not already have it, which
   requires admin privileges.

.. [#f2] When transferring the package cache to the offline machine, it is
   likely that the index JSON files in ``pkgs/cache`` will be invalidated (last
   time modified will probably not match the timestamp inside the file). This
   will leave the solver in a confused state that may still determine a solution
   for the environment, but the solution will probably not be compatible with
   the package cache that was obtained using the original solution on the online
   machine. Since that is the package cache that we will have moved onto the
   offline machine, the install will fail.

**Requirements**:

* A machine connected to the internet that has ``micromamba`` installed
* The ability to invoke the output of ``micromamba shell hook`` on the offline
  machine (should not require admin privileges, but strict group policies may
  prevent script execution altogether)
* The ability to transfer files from the online machine to the offline one
* *Optional*: the ability to unzip a ``.zip`` archive on the offline machine (if
  this is not possible, then simply transfer all files unzipped)
* See [#f1]_ if the target machine is running Windows

On the online machine
*********************

The following steps should be completed on a machine that is connected to the
internet and has ``micromamba`` installed. It does not have to share the same
architecture as the target machine.

Prepare environment spec file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To begin with, the environment needs to be defined using any of the
`specification file <#specification-files>`_ formats mentioned above. The rest
of the steps will assume that the following ``environment.yml`` file is used:

.. code-block:: yaml

  # environment.yml
  name: offline-env
  channels:
    - conda-forge
  dependencies:
    - python=3.12
    - numpy

Any compatible spec file can be used, but note that if the ``channels`` property
is not set in the file, you may have to set it via the ``-c/--channel`` argument
if it is not set elsewhere.

Create a ``conda-lock.yml`` file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. note::

  See the `conda-lock section <#micromamba-conda-lock>`_ for more details on
  ``conda-lock`` outside of the offline install context, and see ``conda-lock
  lock --help`` if your use case is not covered here.

``conda-lock`` allows us to generate an explicit list of *all* packages required
to build an environment for any number of architectures. We can install it using
``micromamba`` in a new environment:

.. code-block::

  micromamba create -n conda-lock -c conda-forge conda-lock
  micromamba activate conda-lock

Next, the ``conda-lock lock`` command can generate the dependency list using any
of the `specification file <#specification-files>`_ formats mentioned above. We
will use the ``environment.yml`` file listed in the previous step.

.. code-block::

  conda-lock lock -f environment.yml --micromamba

By default, this will create a ``conda-lock.yml`` file with entries for the four
most common platforms/architectures/subdirs: ``linux-64``, ``osx-64``,
``osx-arm64``, ``win-64``. If the platform of your *offline* machine is not
included in this list, then list it explicitly using the ``-p, --platform``
option. Note that you can choose a different name for this file with the
``--lockfile`` option or by just renaming it, but it must end in ``-lock.yml``
for ``micromamba`` to properly recognize it. Also note that conda-lock should
find ``micromamba`` automatically and use it, but to be safe, we explicitly
request that it be used via ``--micromamba``.

We can drop out of the environment now:

.. code-block::

  micromamba deactivate conda-lock

Create a new root prefix directory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create a new directory to be used as the root prefix. Using an isolated root
prefix ensures a clean package cache that contains only the packages necessary
to be moved to the new machine. Any clean directory will work. Create the
directory and update the ``MAMBA_ROOT_PREFIX`` environment variable for the
current shell process.

Bash:

.. code-block:: bash

  mkdir offline-environment
  export MAMBA_ROOT_PREFIX="$PWD/offline-environment"

PowerShell:

.. code-block:: powershell

  mkdir offline-environment
  $env:MAMBA_ROOT_PREFIX="$PWD\offline-environment"

.. note::

  If you close your shell during this process for some reason, remember to reset
  the ``MAMBA_ROOT_PREFIX`` environment variable.

Create the environment
^^^^^^^^^^^^^^^^^^^^^^

If the target platform uses a different architecture from the online platform,
the ``MAMBA_PLATFORM`` or ``CONDA_SUBDIR`` environment variables can be used,
but it is easiest to just pass the ``--platform`` argument during environment
creation, which will add a ``.mambarc`` file inside of the new environment's
directory with the ``platform`` property set. We can also download all our
packages without installing them in the creation step by passing the lock file
created earlier to ``-f`` as well as the ``--download-only`` option. For
example, if an online Linux machine is targeting an offline Windows machine, the
following would be used to create and activate the environment:

.. code-block::

  micromamba create -n offline-environment -f conda-lock.yml --download-only --platform win-64

Note that the ``--platform`` chosen above must be one of the ones provided in
the ``conda-lock.yml`` file.

``--download-only`` causes the packages to be downloaded to
``$MAMBA_ROOT_PREFIX/pkgs`` but not linked into
``$MAMBA_ROOT_PREFIX/envs/offline-environment``. It also causes the downloaded
``.conda`` and ``.tar.bz2`` archives to be extracted, which is undesirable for
this use-case since it creates a redundant copy of every package, increasing the
file transfer size. Thus, the extracted directories can be removed if desired:

Bash:

.. code-block:: bash

  find "$MAMBA_ROOT_PREFIX/pkgs" -type d -delete

PowerShell:

.. code-block:: powershell

  ls "$env:MAMBA_ROOT_PREFIX\pkgs" -Attributes Directory | rm -Recurse

``micromamba`` will extract the archives when the environment is reproduced on
the offline machine, so there is no real disadvantage in doing this. Currently,
there is no way to decouple the download and extract steps, but this may change
in the future (see `here <https://github.com/mamba-org/mamba/issues/3251>`_).

Compress the package cache
^^^^^^^^^^^^^^^^^^^^^^^^^^

*This step is optional and just reduces the file transfer size. If the offline
machine does not have a way to unzip* ``.zip`` *archives, then this step should be
skipped.*

With the environment packages downloaded, we can compress the
``$MAMBA_ROOT_PREFIX/pkgs`` directory, commonly called the "package cache". Most
systems provide ``tar`` by default or some other way to unzip archives, but
check to ensure that you can unzip archives on your offline machine. Navigate
into your ``$MAMBA_ROOT_PREFIX`` directory and create a new archive containing
your package cache:

.. code-block::

  cd $MAMBA_ROOT_PREFIX
  tar -czvf pkgs.zip ./pkgs

Prepare files for transfer
^^^^^^^^^^^^^^^^^^^^^^^^^^

Move the following files into your staging area or onto an external drive where
it can be transported to the offline machine:

* The ``pkgs.zip`` archive
* The ``micromamba`` executable *for your target architecture* (see the
  `releases page <https://github.com/mamba-org/micromamba-releases/releases/latest>`_
  to download executables for different architectures)
* The ``conda-lock.yml`` lock file
* Any additional source code required

These can all be zipped as well before transferring if desired.

On the offline machine
**********************

Set up ``micromamba``
^^^^^^^^^^^^^^^^^^^^^

After completing the transfer, you can set up ``micromamba`` manually using
environment variables and ``conda shell hook`` or allow it to set itself up
using

.. code-block::

  ./path/to/micromamba shell init

and restarting your terminal.

.. note::

  On Windows, you may need change your script execution policy using
  ``Set-ExecutionPolicy Unrestricted CurrentUser`` which will apply to your user
  always, or use ``Process`` as the final argument to set it for only this
  process. You may also see a request to enable long filename support after
  running ``micromamba shell init``. This requires admin to enable, but is not
  strictly necessary, although it is certainly recommended if possible. Some
  systems may have this enabled already.

Unzip the package cache
^^^^^^^^^^^^^^^^^^^^^^^

After setting up your new prefix through ``micromamba shell init`` or otherwise,
navigate to it and unzip the packages cache.

.. code-block::

  cd $MAMBA_ROOT_PREFIX
  tar -xzvf /path/to/pkgs.zip

The ``.conda`` and ``.tar.bz2`` package archives should be present under the
``$MAMBA_ROOT_PREFIX/pkgs`` directory now.

Create the new environment
^^^^^^^^^^^^^^^^^^^^^^^^^^

At this point, all that's left is to create the new environment, which will
extract the cached packages and link them into the new environment. Thanks to
``conda-lock``, the environment spec file will match one-to-one with the package
cache, so no downloading or solving is required. Do note that ``conda-lock.yml``
does not specify an environment name, so this must be done manually.

.. code-block::

  micromamba create -n offline-environment -f /path/to/conda-lock.yml

.. note::

  The ``--offline`` argument should not be required in the creation step, but
  note that it does exist in case this breaks without it in the future.
  Additionally, if ``micromamba`` complains about not being able to find the
  ``pkgs_dir``, then add the following lines to your ``.mambarc``:

.. code-block:: yaml

  pkgs_dirs:
    - /path/to/$MAMBA_ROOT_PREFIX/pkgs

The environment should now be working!
