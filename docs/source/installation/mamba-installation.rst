.. _mamba-install:

==================
Mamba Installation
==================

.. warning::
   With the release of Conda 23.10, **Mamba is deprecated**.

   Please do not create any new installations of Mamba.

   Please use :ref:`Micromamba <umamba-install>` or `Miniforge <https://github.com/conda-forge/miniforge>`_ >= `23.3.1-0` instead.


Legacy installation methods (not recommended)
---------------------------------------------

Mambaforge/Miniforge
*********************

You can use the `Miniforge distribution <https://github.com/conda-forge/miniforge>`_ >= `Miniforge3-23.3.1-0` to install Mamba.
If you need an older version of Mamba, please use the Mambaforge distribution instead.

Miniforge comes with the popular ``conda-forge`` channel preconfigured, but you can modify the configuration to use any channel you like.

After successful installation, you can use the mamba commands as described in :ref:`mamba user guide<mamba>`.

.. note::

   1. After installation, please :ref:`make sure <defaults_channels>` that you do not have the Anaconda default channels configured.
   2. Do not install anything into the `base` environment as this might break your installation. See :ref:`here <base_packages>` for details.


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
