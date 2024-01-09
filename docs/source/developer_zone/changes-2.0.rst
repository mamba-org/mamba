=================
Mamba 2.0 Changes
=================
.. ...................... ..
.. THIS IS STILL A DRAFT ..
.. ...................... ..

.. TODO high-level summary of new features:
.. - OCI registries
.. - Mirrors
.. - Own implementation repodata.json
.. - Fully feature implementation of MatchSpec


Command Line Executables
========================
Mamba (executable)
******************
``mamba``, previously a Python executable mixing ``libmambapy``, ``conda``, and code to bridge both
project is being replace by a fully C++ executable based ``libmamba`` solely.

It now presents the same user interface and experience as ``micromamba``.

.. warning::

   The previous code being a highly complex project with few of its original developers still
   active, it is hard to make an exhaustive list of all changes.

   Please help us by reporting changes missing changes.

Micromamba
**********
Micromamba recieves all new features and its CLI remains mostly unchanged.

Breaking changes include:
- ``micromamba shell init`` root prefix parameter ``--prefix`` (``-p``) was renamed
  ``--root-prefix`` (``-r``).
  Both options were supported in version ``1.5``.

.. TODO is micromamba executable renamed mamba?


Libraries
=========
Mamba (Python package)
**********************
In version 2.0, all tools are fully written in C++.
All code previously available in Python through ``import mamba`` has been removed.

Libmambapy (Python bindings)
****************************
The Python bindings to the C++ ``libamamba`` library remain available through ``import libmambapy``.
They are now considered the first class citizen to using Mamba in Python.
Changes inlcude:

- The global ``Context``, previously available through ``Context()``, must now be accessed through
  ``Context.instance()``.
  What's more, it is required to be passed explicitly in a few more functions.
  Future version of ``libmambapy`` will continue in this direction until there are no global context.In version 2, ``Context()`` will throw an exception to avoid hard to catch errors.
- ``ChannelContext`` is no longer an implicit global variable.
  It must be constructed with one of ``ChannelContext.make_simple`` or
  ``ChannelContext.make_conda_compatible`` (with ``Context.instance`` as argument in most cases)
  and passed explicitly to a few functions.
- ``Channel`` has been redesigned an moved to a new ``libmambapy.specs``.
  A featureful ``libmambapy.specs.CondaURL`` is used to describe channel URLs.
  This module also includes ``UndefinedChannel`` used to describe unresolved channel strings.
- ``MatchSpec`` has been redesigned and moved to ``libmambapy.specs``.
  The module also includes a platform enumeration, an implementation of ordered ``Version``, and a
  ``VersionSpec`` to match versions.
- ``PackageInfo`` has been moved to ``libmambapy.specs``.
  Some attributes have been given a more explicit name ``fn`` > ``filename``,
  ``url`` > ``package_url``.

.. TODO include final decision for Channels as URLs.

Libmamba (C++)
**************
The C++ library ``libmamba`` has received significant changes.
Due to the low usage of the C++ interface, all changes are not listed here.
The main changes are:
- Refactoring and testing of a large number of utilities into a ``util::`` namespace,
- Creation of the ``specs::`` with:
    - Implementations of ``Version`` and ``VersionSpec`` for matching versions,
    - A refactoring of a purely funcitonal ``Channel`` class,
    - Implementaiton of a ``UndefinedChannel`` to describe unresolved ``Channels``,
    - A refactored implementation of ``MatchSpec`` using the components above.
- A cleanup of ``ChannelContext`` for be a light proxy and parameter holder wrapping the
  ``specs::Channel``.
- A new ``repodata.json`` parser using `simdjson <https://simdjson.org/>`_.
- Improved downloaders.

.. TODO OCI registry
.. TODO Mirrors
