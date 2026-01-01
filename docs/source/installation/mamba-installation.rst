.. _mamba-install:

==================
Mamba Installation
==================

Fresh install (recommended)
***************************

We recommend that you start with the `Miniforge distribution <https://github.com/conda-forge/miniforge>`_ >= ``Miniforge3-23.3.1-0``.
If you need an older version of Mamba, please use the Mambaforge distribution.
Miniforge comes with the popular ``conda-forge`` channel preconfigured, but you can modify the configuration to use any channel you like.

After successful installation, you can use the mamba commands as described in :ref:`mamba user guide<mamba>`.

.. note::

   1. After installation, please :ref:`make sure <defaults_channels>` that you do not have the Anaconda default channels configured.
   2. Do not install anything into the ``base`` environment as this might break your installation. See :ref:`here <base_packages>` for details.

Docker images
*************

In addition to the Miniforge standalone distribution (see above), there are also the
`condaforge/miniforge3 <https://hub.docker.com/r/condaforge/miniforge3>`_ docker
images:

.. code-block:: bash

  docker run -it --rm condaforge/miniforge3:latest mamba info


Conda libmamba solver
*********************

For a totally conda-compatible experience with the fast Mamba solver,
`conda-libmamba-solver <https://github.com/conda/conda-libmamba-solver>`_ now ships by default with
Conda.
Just use an up to date version of Conda to enjoy the speed improvememts.

.. _mamba-uninstall:

Uninstalling Mamba
******************

Since ``mamba`` is typically installed as part of the `Miniforge distribution <https://github.com/conda-forge/miniforge>`_,
uninstalling ``mamba`` involves removing the entire Miniforge installation.

.. note::

   Before uninstalling, you can check your specific installation paths by running:

   .. code-block:: bash

      mamba info

   This will show you important information such as:

   - ``envs directories``: where your environments are stored
   - ``package cache``: where downloaded packages are cached
   - ``user config files``: location of your ``.mambarc`` file
   - ``populated config files``: location of your ``.condarc`` file
   - ``base environment``: the base Miniforge installation directory (parent of the base env)

   Use these paths to adapt the commands below to your specific installation.

1. Remove shell initialization
   ^^^^^^^^^^^^^^^^^^^^^^^^^^^

   If you initialized your shell with ``mamba shell init``, you need to remove the initialization code from your shell configuration files.
   Run the following command for each shell you initialized:

   .. code-block:: bash

      mamba shell deinit -s bash    # for bash
      mamba shell deinit -s zsh     # for zsh
      mamba shell deinit -s fish    # for fish
      mamba shell deinit -s xonsh   # for xonsh
      mamba shell deinit -s csh     # for csh/tcsh
      mamba shell deinit -s nu      # for nushell

   On Windows PowerShell:

   .. code-block:: powershell

      mamba shell deinit -s powershell

   This will remove the mamba initialization block from your shell configuration files (``.bashrc``, ``.zshrc``, ``config.fish``, etc.).

2. Remove the Miniforge installation directory and package cache
   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   The Miniforge installation directory contains ``mamba``, ``conda``, and all installed packages and environments.
   Check ``mamba info`` to find the exact location:

   - The base environment path shows the Miniforge installation directory (it's the parent of the base env)
   - The package cache location is shown under ``package cache``

   Default locations depend on your operating system:

   - **Linux/macOS:** Usually ``~/miniforge3`` or ``~/mambaforge``
   - **Windows:** Usually ``C:\Users\<username>\miniforge3`` or ``C:\Users\<username>\mambaforge``

   .. code-block:: bash

      # Linux/macOS - remove the installation directory
      # Use the base environment path from 'mamba info' to find the exact location
      rm -rf ~/miniforge3
      # or
      rm -rf ~/mambaforge

      # Also remove the package cache if it's in a separate location
      # (check the 'package cache' path from 'mamba info')
      rm -rf ~/.cache/conda/pkgs  # or your specific cache location

   On Windows PowerShell:

   .. code-block:: powershell

      # Use the base environment path from 'mamba info' to find the exact location
      Remove-Item -Recurse -Force $env:USERPROFILE\miniforge3
      # or
      Remove-Item -Recurse -Force $env:USERPROFILE\mambaforge
      # Also remove package cache if separate (check 'mamba info' for exact path)

   .. warning::

      This will delete the entire Miniforge installation, including all environments, installed packages, and cached data.
      Make sure you have backed up any important environments or data before removing this directory.

3. Remove configuration files (optional)
   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   Check ``mamba info`` for the exact paths to your configuration files:

   - ``user config files`` shows the location of ``.mambarc``
   - ``populated config files`` shows the location of ``.condarc``

   If you want to remove these configuration files:

   .. code-block:: bash

      # Use the paths from 'mamba info', or default locations:
      rm ~/.mambarc        # if it exists
      rm ~/.condarc        # if it exists and was only used for mamba/miniforge

   On Windows:

   .. code-block:: powershell

      # Use the paths from 'mamba info', or default locations:
      Remove-Item $env:USERPROFILE\.mambarc -ErrorAction SilentlyContinue
      Remove-Item $env:USERPROFILE\.condarc -ErrorAction SilentlyContinue

   .. note::

      If you also use ``conda`` from another distribution (like Anaconda), be careful not to delete shared configuration files that are still needed.

4. Remove from PATH (if manually added)
   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   If you manually added Miniforge to your PATH, remove those entries from your shell configuration files (``.bashrc``, ``.zshrc``, ``.profile``, etc.).

After completing these steps, ``mamba`` and the Miniforge distribution will be completely removed from your system.
