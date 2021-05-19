.. _mamba:

Mamba
-----

``mamba`` is a CLI tool to manage ``conda`` s environments.

If you already know ``conda``, great, you already know ``mamba``!

If you're new to this world, don't panic you will find everything you need in this documentation. We recommend to get familiar with :ref:`concepts<concepts>` first.


Quickstarts
===========

The ``mamba create`` command creates a new environment.

You can create an environment with the name ``nameofmyenv`` by calling:

.. code::

    mamba create -n nameofmyenv <list of packages>


After this process has finished, you can _activate_ the virtual environment by calling ``conda activate <nameofmyenv>``.
For example, to install JupyterLab from the ``conda-forge`` channel and then run it, you could use the following commands:

.. code::

    mamba create -n myjlabenv jupyterlab -c conda-forge
    conda activate myjlabenv  # activate our environment
    jupyter lab               # this will start up jupyter lab and open a browser

Once an environment is activated, ``mamba install`` can be used to install further packages into the environment.

.. code::

    conda activate myjlabenv
    mamba install bqplot  # now you can use bqplot in myjlabenv

``mamba`` vs ``conda`` CLIs
===========================

| ``mamba`` is a drop-in replacement and uses the same commands and configuration options as ``conda``.
| You can swap almost all commands between ``conda`` & ``mamba``:

.. code::

   mamba install ...
   mamba create -n ... -c ... ...
   mamba list

.. warning::
    The only difference is that you should still use ``conda`` for :ref:`activation<activation>` and :ref:`deactivation<deactivation>`.


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

    # will show you a tree view of the dependencies of xtensor.
    $ mamba repoquery depends xtensor

.. code::

    $ mamba repoquery depends xtensor

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

    Name            Version Build          Channel
    ──────────────────────────────────────────────────
    ipykernel       5.2.1   py37h43977f1_0 installed
    ipywidgets      7.5.1   py_0           installed
    jupyter_console 6.1.0   py_1           installed

With the ``-t,--tree`` flag, you can get the same information in a tree.
