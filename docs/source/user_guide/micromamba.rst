.. _micromamba:

=====================
Micromamba User Guide
=====================

``micromamba`` is a tiny version of the ``mamba`` package manager.
It is a statically linked C++ executable with a separate command line interface.
It does not need a ``base`` environment and does not come with a default version of Python.

.. note::

   ``mamba`` and ``micromamba`` are the same code base, only build options vary.

In combinations with subcommands like ``micromamba shell`` and ``micromamba run``, it is extremely
convenient in CI and Docker environments where running shell activation hooks is complicated.

It can be used with a drop-in installation without further steps:

.. code-block:: shell

    /path/to/micromamba create -p /tmp/env 'pytest>=8.0'
    /path/to/micromamba run -p /tmp/env pytest myproject/tests

Shell Command
=============

The ``micromamba shell`` command provides functionality for launching shells and managing shell initialization scripts for environment activation.

Launching a Shell
-----------------

When used without subcommands, ``micromamba shell`` launches a new shell session in the specified environment:

.. code-block:: shell

    # Launch a shell in the specified environment
    micromamba shell -p /path/to/env
    micromamba shell -n myenv

This is particularly useful in CI/CD environments and Docker containers where you need to work within a specific environment.

Shell Initialization Management
-------------------------------

The ``micromamba shell`` command also includes several subcommands for managing shell initialization scripts:

``micromamba shell init``
    Add micromamba initialization scripts to your shell's configuration files (e.g., ``.bashrc``, ``.zshrc``).

``micromamba shell deinit``
    Remove micromamba initialization scripts from your shell's configuration files.

``micromamba shell reinit``
    Restore micromamba initialization scripts in your shell's configuration files.

Environment Activation Scripts
------------------------------

These subcommands generate shell-specific activation code that can be used in scripts or sourced directly:

``micromamba shell activate``
    Generate activation code for the specified environment.

``micromamba shell deactivate``
    Generate deactivation code to leave the current environment.

``micromamba shell reactivate``
    Generate reactivation code to refresh the current environment.

``micromamba shell hook``
    Generate shell hook scripts for environment activation.

.. code-block:: shell

    # Generate activation code for bash
    micromamba shell activate -p /path/to/env -s bash
    
    # Generate deactivation code
    micromamba shell deactivate -s bash
    
    # Use in a script
    eval "$(micromamba shell activate -p /path/to/env -s bash)"

Supported Shells
----------------

The shell command supports the following shells:

* ``bash`` - Bash shell
* ``zsh`` - Z shell  
* ``fish`` - Fish shell
* ``powershell`` - PowerShell
* ``cmd.exe`` - Windows Command Prompt
* ``xonsh`` - Xonsh shell
* ``tcsh`` - Tcsh shell
* ``dash`` - Dash shell
* ``nu`` - Nu shell
* ``posix`` - POSIX-compliant shells

Common Options
--------------

``-s, --shell SHELL``
    Specify the shell type. If not provided, micromamba will attempt to detect your shell automatically.

``-p, --prefix PATH``
    Specify the environment by its installation path.

``-n, --name NAME``
    Specify the environment by its name.

``-r, --root-prefix PATH``
    Specify the root prefix (base environment location).

Platform-Specific Features
---------------------------

``micromamba shell enable_long_path_support``
    On Windows, this command enables long path support, which allows working with file paths longer than 260 characters.
