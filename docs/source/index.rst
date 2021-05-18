Welcome to mamba's documentation!
=================================

Mamba is a fast, robust, and cross-platform package manager.

It runs on Windows, OS X and Linux (ARM64 and PPC64LE included) and is fully compatible with ``conda`` packages and supports most of conda's commands.

Mamba offers multiple flavors:

- ``mamba``: a Python-based CLI conceived as a *drop-in* replacement for ``conda``, offering higher speed and more reliable environment solutions
- ``micromamba``: a pure C++-based CLI, self-contained in a single-file executable
- ``libmamba``: a C++ library exposing low-level and high-level APIs on top of which both ``mamba`` and ``micromamba`` are built

.. note::
    :ref:`micromamba<micromamba>` is especially well fitted for the CI use-case but not limited to that!

We recommend :ref:`installation<installation>` from ``mambaforge`` or through ``conda install mamba -n base -c conda-forge``, if you're already a conda user.


.. toctree::
   :caption: INSTALLATION
   :maxdepth: 2
   :hidden:

   installation

.. toctree::
   :caption: USER GUIDE
   :maxdepth: 2
   :hidden:

   user_guide/concepts
   user_guide/mamba
   user_guide/micromamba
   user_guide/configuration

.. toctree::
   :caption: API reference
   :maxdepth: 2
   :hidden:

   python_api

.. toctree::
   :caption: developer zone
   :maxdepth: 2
   :hidden:

   developer_zone/internals


Indices and tables
==================

* {ref}``Index <genindex>``
* {ref}``Search <search>``

<!-- * {ref}``modindex <modindex>`` -->
