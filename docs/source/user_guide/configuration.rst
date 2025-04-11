.. _configuration:

Configuration
=============

Overview
--------

While ``mamba`` currently relies on ``conda`` configuration, ``libmamba`` and downstream projects such as ``micromamba`` or ``rhumba``
rely on a pure C++ implementation.

.. note::
  For ``mamba`` configuration, please refer to `conda documentation <https://docs.conda.io/projects/conda/en/latest/user-guide/configuration/index.html>`_

The configuration is parsed/read from multiple sources types:

- **rc file**: a file using ``YAML`` syntax
- **environment variable**: a key/value pair set prior to mamba execution
- **CLI**: a parsed argument/option from a CLI interface
- **API**: a value set programmatically by a program relying on mamba

The precedence order between those sources is:

.. image:: config_srcs.svg
  :width: 600
  :align: center

.. note::
  ``rc`` file stands historically for ``run commands`` which could also translate to
  ``runtime configuration``.
  It's a convenient way to persist configuration on the filesystem and use it as default.


.. _precedence-resolution:

Precedence resolution
---------------------

Depending on its type, the resolution of a configuration value between multiple sources is:

- scalar (boolean, string): value from highest precedence source
- sequence (list): appended from highest precedence source to lowest

Example:

Running ``micromamba install xtensor -c my-channel`` with 3 sources of configuration:

- ``channels`` and ``always_yes`` set from rc file located at ``~/.mambarc``.
- ``channels`` set from CLI using ``-c`` option.
- ``always_yes`` set from environment variable using ``MAMBA_ALWAYS_YES`` env var

.. code::

  $ cat ~/.mambarc
  channels:
    - conda-forge
  always_yes: false

.. code::

  $ echo $MAMBA_ALWAYS_YES
  true

The resulting configuration written using ``YAML`` syntax is:

.. code::

  $ micromamba config list --sources
  channels:
    - my-channel  # 'CLI'
    - conda-forge  # '~/.mambarc'
  always_yes: true  # 'MAMBA_ALWAYS_YES' > '~/.mambarc'


Multiple rc files
-----------------

A user may have multiple rc files located at different places on their machine.

It's a convenient way for configuration to apply in given scopes:

- system wide: all users
- root prefix: all environments sharing the same root prefix
- current user
- target prefix: a specific target prefix | environment
- a single use

Alternatively, ones could also pass one or more rc files to use from the CLI. Other sources are then ignored.

RC files have their own precedence order and use the same resolution process as other configuration sources:

.. image:: config_rc_srcs.svg
  :width: 800
  :align: center

.. code::

        // on_unix
        {
        "/etc/conda/.condarc",
        "/etc/conda/condarc",
        "/etc/conda/condarc.d/",
        "/etc/conda/.mambarc",
        "/var/lib/conda/.condarc",
        "/var/lib/conda/condarc",
        "/var/lib/conda/condarc.d/",
        "/var/lib/conda/.mambarc"
        }
        // on_win
        {
        "C:\\ProgramData\\conda\\.condarc",
        "C:\\ProgramData\\conda\\condarc",
        "C:\\ProgramData\\conda\\condarc.d",
        "C:\\ProgramData\\conda\\.mambarc"
        }

        { root_prefix }/.condarc
        { root_prefix }/condarc
        { root_prefix }/condarc.d
        { root_prefix }/.mambarc

	{ $XDG_CONFIG_HOME | ~/.config}/conda/.condarc
	{ $XDG_CONFIG_HOME | ~/.config}/conda/condarc
	{ $XDG_CONFIG_HOME | ~/.config}/condarc.d
        ~/.conda/.condarc
        ~/.conda/condarc
        ~/.conda/condarc.d
        ~/.condarc

        { $XDG_CONFIG_HOME | ~/.config}/mamba/.mambarc
        { $XDG_CONFIG_HOME | ~/.config}/mamba/mambarc
	{ $XDG_CONFIG_HOME | ~/.config}/mamba/mambarc.d
        ~/.mamba/.mambarc
        ~/.mamba/mambarc
        ~/.mamba/mambarc.d
        ~/.mambarc

        { target_prefix }/.condarc
        { target_prefix }/condarc
        { target_prefix }/condarc.d
        { target_prefix }/.mambarc

        $CONDARC,
        $MAMBARC;
