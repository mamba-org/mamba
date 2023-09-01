.. _mamba:

Mamba User Guide
----------------

``mamba`` is a CLI tool to manage ``conda`` s environments.

If you already know ``conda``, great, you already know ``mamba``!

If you're new to this world, don't panic you will find everything you need in this documentation. We recommend to get familiar with :ref:`concepts<concepts>` first.


Quickstart
==========

The ``mamba create`` command creates a new environment.

You can create an environment with the name ``nameofmyenv`` by calling:

.. code::

    mamba create -n nameofmyenv <list of packages>


After this process has finished, you can _activate_ the virtual environment by calling ``mamba activate <nameofmyenv>``.
For example, to install JupyterLab from the ``conda-forge`` channel and then run it, you could use the following commands:

.. code::

    mamba create -n myjlabenv jupyterlab -c conda-forge
    mamba activate myjlabenv  # activate our environment
    jupyter lab               # this will start up jupyter lab and open a browser

Once an environment is activated, ``mamba install`` can be used to install further packages into the environment.

.. code::

    mamba activate myjlabenv
    mamba install bqplot  # now you can use bqplot in myjlabenv
    mamba install "matplotlib>=3.5.0" cartopy  # now you installed matplotlib with version>=3.5.0 and default version of cartopy

``mamba`` vs ``conda`` CLIs
===========================

| ``mamba`` is a drop-in replacement and uses the same commands and configuration options as ``conda``.
| You can swap almost all commands between ``conda`` & ``mamba``:

.. code::

   mamba install ...
   mamba create -n ... -c ... ...
   mamba list

Specification files
===================

``mamba`` supports the same environment specification file formats as ``conda``.

.. important::

   While ``micromamba`` :ref:`supports conda-lock "unified" lock
   files<micromamba-conda-lock>`, Mamba currently does not.

Repoquery
=========

``mamba`` comes with features on top of stock ``conda``.
To efficiently query repositories and query package dependencies you can use ``mamba repoquery``.

Here are some examples:

.. code::

    # will show you all available xtensor packages.
    $ mamba repoquery search xtensor

    # you can also specify more constraints on this search query
    $ mamba repoquery search "xtensor>=0.18"

    # will show you a list of the direct dependencies of xtensor.
    $ mamba repoquery depends xtensor

    # will show you a list of the dependencies (including dependencies of dependencies).
    $ mamba repoquery depends xtensor --recursive

The flag ``--recursive`` shows also recursive (i.e. transitive) dependencies of dependent packages instead of only direct dependencies.
With the ``-t,--tree`` flag, you can get the same information of a recursive query in a tree.

.. code::

    $ mamba repoquery depends -t xtensor

    xtensor == 0.21.5
    ├─ libgcc-ng [>=7.3.0]
    │ ├─ _libgcc_mutex [0.1 conda_forge]
    │ └─ _openmp_mutex [>=4.5]
    │   ├─ _libgcc_mutex already visited
    │   └─ libgomp [>=7.3.0]
    │     └─ _libgcc_mutex already visited
    ├─ libstdcxx-ng [>=7.3.0]
    └─ xtl [>=0.6.9,<0.7]
        ├─ libgcc-ng already visited
        └─ libstdcxx-ng already visited


And you can ask for the inverse, which packages depend on some other package (e.g. ``ipython``) using ``whoneeds``.

.. code::

    $ mamba repoquery whoneeds ipython

    Name            Version Build          Depends          Channel
    -------------------------------------------------------------------
    jupyter_console 6.4.3   pyhd3eb1b0_0   ipython          pkgs/main
    ipykernel       6.9.1   py39haa95532_0 ipython >=7.23.1 pkgs/main
    ipywidgets      7.6.5   pyhd3eb1b0_1   ipython >=4.0.0  pkgs/main


With the ``-t,--tree`` flag, you can get the same information in a tree.

.. code::

    $ mamba repoquery whoneeds -t ipython

    ipython[8.2.0]
    ├─ jupyter_console[6.4.3]
    │  └─ jupyter[1.0.0]
    ├─ ipykernel[6.9.1]
    │  ├─ notebook[6.4.8]
    │  │  ├─ widgetsnbextension[3.5.2]
    │  │  │  └─ ipywidgets[7.6.5]
    │  │  │     └─ jupyter already visited
    │  │  └─ jupyter already visited
    │  ├─ jupyter_console already visited
    │  ├─ ipywidgets already visited
    │  ├─ jupyter already visited
    │  └─ qtconsole[5.3.0]
    │     └─ jupyter already visited
    └─ ipywidgets already visited


.. note::
  ``depends`` and ``whoneeds`` sub-commands require either the specified package to be installed in you environment, or for the channel to be specified with the ``-c,--channel`` flag.
  When ``search`` sub-command is used without specifying the **channel** explicitly (using the flag previously mentioned), the search will be performed considering the channels set during the configuration.
