.. _concepts:

Concepts
--------

Few concepts are extensively used in ``Mamba`` and in this documentation as well.
| You should start by getting familiar with those as a starting point.


.. _prefix:

Prefix/Environment
==================


In Unix-like platforms, installing a software consists in placing files in subdirectories of an "installation prefix":

.. image:: prefix.png
  :height: 300
  :align: center

- no file is placed outside of the installation *prefix*
- dependencies must be installed in the same *prefix* (or standard system prefixes with lower precedence)

.. note::
    Examples on Unix: the root of the filesystem, the ``/usr/`` and ``/usr/local/`` directories.

| A *prefix* is a fully self-contained and portable installation.
| To disambiguate with :ref:`root prefix<root-prefix>`, *prefix* is often called *target prefix*. Without explicit *target* or *root* reference, you can assume it points a *target prefix*.

An *environment* is just another way to call a *target prefix*.

Mamba's environments are similar to virtual environments known from Python's ``virtualenv`` and similar software, but more powerful since Mamba also manages *native* dependencies and generalizes the virtual environment concept to many programming languages.

.. _root-prefix:

Root prefix
===========

When downloading for the first time the index of packages for resolution of the environment, or the packages themselves, a *cache* is generated to speed-up next operations:

- the index has a :ref:`configurable<configuration>` time-to-live (TTL) during which it will be considered as valid
- the packages are preferentially hard-linked to the *cache* location

This *cache* is shared by all *environments* or *target prefixes* based on the same *root prefix*. Basically, that *cache* directory is a subdirectory located at ``$root_prefix/pkgs/``.

The *root prefix* also provide a convenient structure to store *environments* ``$root_prefix/envs/``, even if you are free to create an *environment* elsewhere.


.. _base-env:

Base environment
================

The *base* environment is the environment located at the *root prefix*.

| This is a legacy *environment* from ``conda`` implementation still heavily used.
| The *base* environment acts as an intermediate :ref:`system prefix<prefix>` with lower precedence than the :ref:`activated<activation>` environement: ``system prefix < base < env``.

 | ``mamba`` and ``conda`` being themselves Python packages, they are installed in *base* environment, making the CLIs available in all *activated* environment *based* on this *base* environement.

.. note::
  It also works for any other tool or lib installed in *base*.

.. note::
  You can't ``create`` *base* environment because it's already part of the *root prefix* structure, directly ``install`` in *base* instead.


Activation/Deactivation
=======================

.. _activation:

Activation
**********

The *activation* of an :ref:`environment<prefix>` makes all its content available to your shell. It mainly adds *target prefix* subdirectories to your ``$PATH`` environment variable.

.. note::
  *activation* implementation is platform dependent (see ``LD_LIBRARY_PATH``).

| When *activating* an environment from another, you can choose to ``stack`` or not upon the currently activated env.
| Stacking will result in a new intermediate :ref:`prefix<prefix>`: ``system prefix < base < env1 < env2``.


.. _deactivation:

Deactivation
************

The *deactivation* is the opposite operation of :ref:`activation<activation>`, removing from your shell what makes the environment content accessible.
