Welcome to Mamba's documentation!
=================================

Mamba is a fast, robust, and cross-platform package manager.

It runs on Windows, OS X and Linux (ARM64 and PPC64LE included) and is fully compatible with ``conda`` packages and supports most of conda's commands.

The ``mamba-org`` organization hosts multiple Mamba flavors:

- ``mamba``: a Python-based CLI conceived as a *drop-in* replacement for ``conda``, offering higher speed and more reliable environment solutions
- ``micromamba``: a pure C++-based CLI, self-contained in a single-file executable
- ``libmamba``: a C++ library exposing low-level and high-level APIs on top of which both ``mamba`` and ``micromamba`` are built

.. note::
   In this documentation, ``Mamba`` will refer to all flavors while flavor-specific details will mention ``mamba``, ``micromamba`` or ``libmamba``.

.. note::
    :ref:`micromamba<micromamba>` is especially well fitted for the CI use-case but not limited to that!

You can try Mamba now by visiting the :ref:`installation page<installation>`!


.. toctree::
   :caption: INSTALLATION
   :maxdepth: 2
   :hidden:

   installation
   mamba-installation
   micromamba-installation

.. toctree::
   :caption: USER GUIDE
   :maxdepth: 2
   :hidden:

   user_guide/concepts
   user_guide/mamba
   user_guide/micromamba
   user_guide/configuration
   user_guide/troubleshooting

.. toctree::
   :caption: ADVANCED USAGE
   :maxdepth: 2
   :hidden:

   advanced_usage/more_concepts
   advanced_usage/detailed_operations
   advanced_usage/artifacts_verification
   advanced_usage/package_resolution

.. toctree::
   :caption: API reference
   :maxdepth: 2
   :hidden:

   python_api

.. toctree::
   :caption: developer zone
   :maxdepth: 2
   :hidden:

   developer_zone/contributing
   developer_zone/build_locally
   developer_zone/internals
