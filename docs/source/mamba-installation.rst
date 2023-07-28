.. _mamba-install:
==================
Mamba Installation
==================

Fresh install
*************

For a pre-configured installation, minimally different from ``conda``, the Mambaforge distribution
is a community project of the conda-forge community that packages ``mamba``.

| You can download `Mambaforge <https://github.com/conda-forge/miniforge#mambaforge>`_ for Windows, macOS and Linux.
| Mambaforge comes with the popular ``conda-forge`` channel preconfigured, but you can modify the configuration to use any channel you like.

 | After successful installation, you can use the mamba commands as described in :ref:`mamba user guide<mamba>`.

.. note::
   For both ``mamba`` and ``conda``, the ``base`` environment is meant to hold their dependencies.
   It is strongly discouraged to install anything else in the base envionment.
   Doing so may break ``mamba`` and ``conda`` installation.


Existing ``conda`` install (not recommended)
********************************************

.. warning::
   This way of installing Mamba is **not recommended**.
   We strongly recommend to use the Mambaforge method (see above).

To get ``mamba``, just install it *into the base environment* from the ``conda-forge`` channel:

.. code:: bash

   # NOT RECOMMENDED: This method of installation is not recommended, prefer Mambaforge instead (see above)
   # conda install -n base --override-channels -c conda-forge mamba 'python_abi=*=*cp*'


.. warning::
   Installing mamba into any other environment than ``base`` is not supported.
