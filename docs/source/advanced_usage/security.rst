.. _security:

Security
========

Package link scripts
--------------------

Some packages include ``pre-link``, ``post-link``, ``pre-unlink``, or
``post-unlink`` scripts that execute during package installation, update
or removal.

Note that ``post-unlink`` scripts are **deprecated** and therefore
not executed.

These scripts run in a shell subprocess and can perform arbitrary
operations on the system.

``pre-link`` scripts are particularly high-risk: they run from the
package cache and can modify the package itself, affecting all
environments that use that cached package.

By default, Mamba shows a security warning in case a transaction
involves packages with scripts and prompts for confirmation.

Disabling link scripts
^^^^^^^^^^^^^^^^^^^^^^

You can disable execution of all link scripts using the
``--skip-run-link-scripts`` flag:

.. code-block:: bash

   mamba install somepackage --skip-run-link-scripts
   mamba remove somepackage --skip-run-link-scripts

Or via configuration:

.. code-block:: yaml

   # ~/.mambarc
   skip_run_link_scripts: true

.. code-block:: bash

   export MAMBA_SKIP_RUN_LINK_SCRIPTS=true

When disabled, scripts are silently skipped and the security warning
is suppressed.

.. note::
   Some packages rely on link scripts for correct setup.
   Disable only when you understand the trade-offs, such as when
   installing from untrusted sources.

Excluding recent package builds
-------------------------------

By default, package resolution considers every build currently published
in the configured channels. ``--exclude-newer`` narrows this view to
packages whose policy timestamp is no later than a given cutoff, meaning
the solve only sees packages that existed up to a known-good point in time.

An absolute cutoff (date or datetime) makes solving reproducible at a point in
time; relative durations (e.g. ``7d``) instead form a rolling window. It also
keeps packages rebuilt or republished after a version you verified from being
silently picked up, provided the channel ``indexed_timestamp`` is trustworthy.

Accepted value formats
^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Format
     - Example
     - Cutoff meaning
   * - Compact duration
     - ``7d``, ``3d12h``, ``1y6M7d``
     - *now* minus the duration (``y`` years, ``M`` months, ``w`` weeks,
       ``d`` days, ``h`` hours, ``m`` minutes, ``s`` seconds)
   * - ISO 8601 duration
     - ``P7D``, ``PT24H``, ``P1DT12H``, ``P3Y6M4DT12H30M5S``
     - *now* minus the duration
   * - Plain seconds
     - ``3600``
     - *now* minus N seconds
   * - Date
     - ``2026-04-01``
     - The given day is **included**: the cutoff is the start of the next
       UTC day (i.e ``2026-04-02 00:00:00 UTC``)
   * - Datetime
     - ``2026-04-01T12:00:00Z``
     - The exact instant; naive values are interpreted as UTC

``0``, ``0d``, or ``P0D`` resolve to the current time, effectively disabling
the delay.

Command line
^^^^^^^^^^^^

.. code-block:: bash

   mamba install xtensor --exclude-newer 2026-04-01
   mamba update --all --exclude-newer 7d

Configuration
^^^^^^^^^^^^^

The policy can also be set via an ``rc`` file or environment variables:

.. code-block:: yaml

   # ~/.mambarc (or ~/.condarc)
   exclude_newer: 2026-04-01

.. code-block:: bash

   export MAMBA_EXCLUDE_NEWER=7d   # or: export CONDA_EXCLUDE_NEWER=7d

Per-package overrides
^^^^^^^^^^^^^^^^^^^^^

A package can be pinned to its own cutoff, or exempted entirely, with
``exclude_newer_package``:

.. code-block:: bash

   mamba install numpy pandas --exclude-newer 7d \
     --exclude-newer-package '{"numpy": "false", "pandas": "30d"}'

.. code-block:: yaml

   # ~/.mambarc (or ~/.condarc)
   exclude_newer_package:
     numpy: false
     pandas: 30d

.. code-block:: bash

   export MAMBA_EXCLUDE_NEWER_PACKAGE='{"numpy": "false", "pandas": "30d"}'  # or use CONDA_EXCLUDE_NEWER_PACKAGE

``false`` exempts the package from the global policy. A package-specific
value takes precedence over the global cutoff.

.. note::
   The policy is currently scoped either globally (``exclude_newer``) or
   per-package (``exclude_newer_package``). Per-channel scoping — e.g. delaying
   packages from specific channels while exempting others, is not implemented yet.

Which timestamp is compared
^^^^^^^^^^^^^^^^^^^^^^^^^^^

For repodata JSON, the indexed timestamp (``indexed_timestamp``, set by the
channel server when the artifact is first indexed into the repository) is
preferred over the build ``timestamp`` when both are present
(see `CEP 47 <https://github.com/conda/ceps/blob/main/cep-0047.md>`_).
``timestamp`` is set by the package builder and is therefore not verifiable;
``mamba`` falls back to it only when ``indexed_timestamp`` is absent.
A package is excluded when its policy timestamp is strictly greater than
the cutoff.

.. note::
   ``exclude_newer`` filtering happens while repodata is parsed with the
   ``mamba`` (simdjson) parser, which is the default. The binary ``.solv``
   cache bypasses this filtering, so ``mamba`` skips it and re-reads repodata
   whenever the ``exclude_newer`` policy is set. Switching to the libsolv
   parser (``--no-mamba-repodata-parsing`` or ``mamba_repodata_parsing: false``)
   disables filtering entirely and ``mamba`` logs a warning in that case.
