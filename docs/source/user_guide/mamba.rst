.. _mamba:

Mamba
-----

``mamba`` is a CLI tool to manage ``conda`` s environments.

| If you already know ``conda``, great, you already know ``mamba``!
| If you're new to this world, don't panic you will find everything you need in this documentation. You recommend you to get familiar with :ref:`concepts<concepts>` first.


Create an environment
=====================

Mamba handles multiple environments. The initial environment is called the *base* environment and in general, you should not install packages into the base environment. Rather it's good practice to create new environments whenever you work on a specific project. Mamba has advanced file deduplication to keep the memory footprint of having multiple environments low.

The ``mamba create`` command creates a new environment. Mamba's environments are similar to virtual environments known from Python's ``virtualenv`` and similar software, but more powerful since mamba also manages *native* dependencies and generalizes the virtual environment concept to many programming languages.

You can create a virtual environment with the name ``nameofmyenv`` by calling

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
    The only difference is that you should still use ``conda`` for *activation* and *deactivation*
