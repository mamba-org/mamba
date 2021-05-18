.. _installation:

============
Installation
============

.. _mamba-install:

Mamba
=====

For new users
*************

We strongly recommend to start from ``mambaforge``, a community project of the conda-forge community.
You can download ``mambaforge`` for `Windows, macOS and Linux <https://github.com/conda-forge/miniforge#mambaforge>`_.
Mambaforge comes with the popular ``conda-forge`` channel preconfigured, but you can modify the configuration to use any channel you like.

After successful installation, you can use the mamba commands as described in :ref:`mamba user guide<mamba>`.


For conda users
***************

If you are already a ``conda`` user, very good! Using ``mamba`` will feel natural.

To get ``mamba``, install it *into the base environment* from the ``conda-forge`` channel:

.. code::

   conda install mamba -n base -c conda-forge


.. warning::
   Installing mamba into any other environment can cause unexpected behavior
