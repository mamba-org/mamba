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
- A new config `order_solver_request` (default true) can be used to order the dependencies passed
  to the solver, getting order independent solutions.

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
  Future version of ``libmambapy`` will continue in this direction until there are no global context.
  In version 2, ``Context()`` will throw an exception to avoid hard to catch errors.
- ``ChannelContext`` is no longer an implicit global variable.
  It must be constructed with one of ``ChannelContext.make_simple`` or
  ``ChannelContext.make_conda_compatible`` (with ``Context.instance`` as argument in most cases)
  and passed explicitly to a few functions.
- A new ``Context`` independent submodule ``libmambapy.specs`` has been introduced with:
  - The redesign of the ``Channel`` and a new ``UnresolvedChannel`` used to describe unresolved
    channel strings.
    A featureful ``libmambapy.specs.CondaURL`` is used to describe channel URLs.
  - The redesign``MatchSpec``.
    The module also includes a platform enumeration, an implementation of ordered ``Version``,
    and a ``VersionSpec`` to match versions.
  - ``PackageInfo`` has been moved to this submodule.
    Some attributes have been given a more explicit name ``fn`` > ``filename``,
    ``url`` > ``package_url``.
- A new ``Context`` independent submodule ``libmambapy.solver`` has been introduced with the
  changes below.
  A usage documentation page is available at
  https://mamba.readthedocs.io/en/latest/usage/solver.html
  - The redesign of the ``Pool``, which is now available as ``libmambapy.solver.libsolv.Database``.
    The new interfaces makes it easier to create repositories without using other ``libmambapy``
    objects.
  - ``Repo`` has been redesigned into a lightweight ``RepoInfo`` and moved to
    ``libmambapy.solver.libsolv``.
    The creation and modification of repos happens through the ``Database``, with methods such as
    ``Database.add_repo_from_repodata_json`` and ``Database.add_repo_from_packages``, but also
    high-level free functions such as ``load_subdir_in_database`` and
    ``load_installed_packages_in_database``.
  - The ``Solver`` has been moved to ``libmambapy.solver.libsolv.Solver``.
    - All jobs, pins, and flags must be passed as a single ``libmambapy.solver.Request``.
    - The outcome of solving the request is either a ``libmambapy.solver.Solution`` or a
      ``libmambapy.solver.libsolv.Unsolvable`` state from which rich error messages can be
      extracted.

.. TODO include final decision for Channels as URLs.

For many changes, an exception throwing placeholder has ben kept to advise developpers on the new
direction to take.

Libmamba (C++)
**************
The C++ library ``libmamba`` has received significant changes.
Due to the low usage of the C++ interface, all changes are not listed here.
The main changes are:

- Refactoring and testing of a large number of utilities into a ``util::`` namespace,
- Creation of the ``specs::`` with:
    - Implementations of ``Version`` and ``VersionSpec`` for matching versions,
    - A refactoring of a purely funcitonal ``Channel`` class,
    - Implementaiton of a ``UnresolvedChannel`` to describe unresolved ``Channels``,
    - A refactored implementation of ``MatchSpec`` using the components above.
- A cleanup of ``ChannelContext`` for be a light proxy and parameter holder wrapping the
  ``specs::Channel``.
- A new ``repodata.json`` parser using `simdjson <https://simdjson.org/>`_.
- The ``MPool``, ``MRepo`` and ``MSolver`` API has been completely redesigned into a ``solver``
  subnamespace and works independently of the ``Context``.
  The ``solver::libsolv`` sub-namespace has also been added for full isolation of libsolv, and a
  solver API without ``Context``.
  The ``solver`` API redesign includes:
    - A refactoring of the ``MPool`` as a ``DataBase``, fully isolates libsolv, and simplifies
      repository creation.
    - A refactoring and thinning of ``MRepo`` as a new ``RepoInfo``.
    - A solver ``Request`` with all requirements to solve is the new way to specify jobs.
    - A refactoring of ``Solver``.
    - A solver outcome as either a ``Solution`` or an ``UnSolvable`` state.
  A usage documentation (in Python) is available at
  https://mamba.readthedocs.io/en/latest/usage/solver.html
- Improved downloaders.

.. TODO OCI registry
.. TODO Mirrors
