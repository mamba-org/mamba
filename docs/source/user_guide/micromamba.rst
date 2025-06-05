.. _micromamba:

=====================
Micromamba User Guide
=====================

``micromamba`` is a tiny version of the ``mamba`` package manager.
It is a statically linked C++ executable with a separate command line interface.
It does not need a ``base`` environment and does not come with a default version of Python.

.. note::

   ``mamba`` and ``micromamba`` are the same code base, only build options vary.

In combinations with subcommands like ``micromamba shell`` and ``micromamba run``, it is extremely
convenient in CI and Docker environments where running shell activatio hooks is complicated.

I can be used with a drop-in installation without further steps:

.. code-block:: shell

    /path/to/micromamba create -p /tmp/env 'pytest>=8.0'
    /path/to/micromamba run -p /tmp/env pytest myproject/tests
