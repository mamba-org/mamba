.. _mamba-install:

==================
Mamba Installation
==================

Fresh install (recommended)
***************************

We recommend that you start with the `Miniforge distribution <https://github.com/conda-forge/miniforge>`_ >= ``Miniforge3-23.3.1-0``.
If you need an older version of Mamba, please use the Mambaforge distribution.
Miniforge comes with the popular ``conda-forge`` channel preconfigured, but you can modify the configuration to use any channel you like.

After successful installation, you can use the mamba commands as described in :ref:`mamba user guide<mamba>`.

.. note::

   1. After installation, please :ref:`make sure <defaults_channels>` that you do not have the Anaconda default channels configured.
   2. Do not install anything into the ``base`` environment as this might break your installation. See :ref:`here <base_packages>` for details.


Existing ``conda`` install (not recommended)
********************************************

.. warning::
   This way of installing Mamba is **not recommended**.
   We strongly recommend to use the Miniforge method (see above).

To get ``mamba``, just install it *into the base environment* from the ``conda-forge`` channel:

.. code:: bash

   # NOT RECOMMENDED: This method of installation is not recommended, prefer Miniforge instead (see above)
   # conda install -n base --override-channels -c conda-forge mamba 'python_abi=*=*cp*'


.. warning::
   Installing mamba into any other environment than ``base`` is not supported.


Docker images
*************

In addition to the Miniforge standalone distribution (see above), there are also the
`condaforge/miniforge3 <https://hub.docker.com/r/condaforge/miniforge3>`_ docker
images:

.. code-block:: bash

  docker run -it --rm condaforge/miniforge3:latest mamba info


Conda libmamba solver
*********************

For a totally conda-compatible experience with the fast Mamba solver,
`conda-libmamba-solver <https://github.com/conda/conda-libmamba-solver>`_ now ships by default with
Conda.
Just use an up to date version of Conda to enjoy the speed improvememts.
