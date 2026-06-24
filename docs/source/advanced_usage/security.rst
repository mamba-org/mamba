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
