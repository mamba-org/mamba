Welcome to Mamba's documentation!
=================================

Mamba is a fast, robust, and cross-platform package manager for Conda packages.

The ``mamba-org`` organization hosts multiple Mamba flavors:

- Micromamba: a pure C++-based CLI, self-contained in a single-file executable.
- libmamba: a C++ library exposing low-level and high-level APIs on top of which both Mamba and Micromamba are built.
- (Mamba: Legacy software; a Python-based CLI conceived as a *drop-in* replacement for Conda, offering higher speed.)

In this documentation, "Mamba" will refer to all flavors while flavor-specific details will mention Mamba, Micromamba, or libmamba.

You can try Mamba now by visiting the installation for
:ref:`mamba<mamba-install>` or :ref:`micromamba<umamba-install>`.


.. toctree::
   :caption: INSTALLATION
   :maxdepth: 2
   :hidden:

   Mamba <installation/mamba-installation>
   Micromamba <installation/micromamba-installation>

.. toctree::
   :caption: USER GUIDE
   :maxdepth: 2
   :hidden:

   user_guide/concepts
   Mamba <user_guide/mamba>
   Micromamba <user_guide/micromamba>
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
   developer_zone/dev_environment
   developer_zone/internals
