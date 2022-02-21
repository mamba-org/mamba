.. _glossary:

Glossary
--------

* :ref:`activate/deactivate`
* :ref:`boa`
* :ref:`channels`
* :ref:`gator`
* :ref:`mamba_env`
* :ref:`metapackage`
* :ref:`miniforge`
* :ref:`noarch_package`
* :ref:`package_manager`
* :ref:`package_repo`
* :ref:`package`
* :ref:`quetz`
* :ref:`repository`
* :ref:`rhumba`

.. _activate/deactivate:

Activate/Deactivate environment
===============================

Mamba uses the ``conda activate`` and ``conda deactivate`` commands from Conda. They're used to switch or move between installed environments. The ``conda activate`` command prepends the path of your current environment to the PATH environment variable so that you do not need to type it each time. ``conda deactivate`` removes it.

.. _boa:

boa
===

Boa is a package build tool for .conda packages. As such, it is an alternative to conda-build, but uses the more recently developed mamba package installer as a “backend”. Additionally, boa implements a new and improved recipe spec, and also implements a conda mambabuild ... command to build “legacy” recipes with the faster mamba backend. You can learn more about boa `here <https://boa-build.readthedocs.io/en/latest/>`_

.. _channels:

Channels
========

Channels are the sources where packages are downloaded from. You can check your channels configuration in your `.condarc` file or set a channel using the argument ``-c [channel]``.

Miniforge sets ``conda-forge`` as the default (and only) channel.

.. _conda_package:

Conda package
=============

A compressed file that contains everything that a software program needs in order to be installed and run, so that you do not have to manually find and install each dependency separately.

.. _gator:

Gator
=====

The Mamba Navigator, it's a Web UI for managing conda environments. It provides Conda/Mamba environment and package management as a standalone application or as extension for JupyterLab.

.. _mamba_env:

Mamba environment
=================

Directory that contains a collection of packages downloaded by the user and their dependencies.

.. _metapackage:

Metapackage
===========

A metapackage is a very simple package that has at least a name and a version. It need not have any dependencies or build steps. Metapackages may list dependencies to several core, low-level libraries and may contain links to software files that are automatically downloaded when executed.

.. _miniforge:

Miniforge
=========

Is a minimal installer for Conda specific to conda-forge. It can be compared to the Miniconda installer.

.. _noarch_package:

Noarch package
==============

Noarch packages are packages that are not architecture specific and therefore only have to be built once.

.. _package_manager:

Package manager
===============

Software that automates the process of installing, removing, updating and configuring packages. Mamba is a package manager, micromamba too.

.. _package:

Package
=======

Software files encapsulated in a file that can be manipulated by a package manager.

.. _quetz:

Quetz
=====

The quetz project is an open source server for conda packages.

.. _repository:

Repository
==========

Storage location for code. * :ref:`quetz` is an example of code repository.

.. _rhumba:

Rhumba
======

Rhumba is an R package manager powered by Mamba.
