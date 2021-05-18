.. _concepts:

Concepts
--------

Few concepts are broadly used in ``mamba`` and in this documentation as well.
| You should start by getting familiar with those as a starting point.


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

| A *prefix* is a fully self-contained and portable installation (thanks to advanced relocation technique).
| To disambiguate with :ref:`root prefix<root-prefix>`, *prefix* is often called *target prefix*. Without explicit *target* or *root* reference, you can assume it points a *target prefix*.

An *environment* is just another way to call a *target prefix*. 

.. _root-prefix:

Root prefix
===========

When downloading for the first time the index of packages for resolution of the environment, or the packages themselves, a *cache* is generated to speed-up next operations:

- the index has a :ref:`configurable<configuration>` time-to-live (TTL) during which it will be considered as valid
- the packages are preferentially hard-linked to the *cache* location

This *cache* is shared by all ``environments`` or ``prefixes`` based on the same ``root prefix``. Basically, that *cache* directory is a subdirectory located at ``$root_prefix/pkgs``.

