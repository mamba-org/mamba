.. _mamba_usage_solver:

Solving Package Environments
============================

.. |MatchSpec|   replace:: :cpp:type:`MatchSpec <mamba::specs::MatchSpec>`
.. |PackageInfo| replace:: :cpp:type:`PackageInfo <mamba::specs::PackageInfo>`
.. |Database|    replace:: :cpp:type:`Database <mamba::solver::libsolv::Database>`
.. |Request|     replace:: :cpp:type:`Request <mamba::solver::Request>`
.. |Solver|      replace:: :cpp:type:`Solver <mamba::solver::libsolv::Solver>`
.. |Solution|    replace:: :cpp:type:`Solution <mamba::solver::Solution>`
.. |UnSolvable|  replace:: :cpp:type:`UnSolvable <mamba::solver::libsolv::UnSolvable>`

The :any:`libmambapy.solver <mamba::solver>` submodule contains a generic API for solving
requirements (|MatchSpec|) into a list of packages (|PackageInfo|) with no conflicting dependencies.
This problem is hard to solve (`NP-complete <https://en.wikipedia.org/wiki/NP-completeness>`_) which
is why Mamba uses a `SAT solver <https://en.wikipedia.org/wiki/SAT_solver>`_ to do so.

.. note::

   There is currently only one solver available in Mamba:
   `LibSolv <https://en.opensuse.org/openSUSE:Libzypp_satsolver>`_. For this reason, the generic
   interface has not been fully completed and users need to access the submodule
   :any:`libmambapy.solver.libsolv <mamba::solver::libsolv>` for certain types.

Populating the Package Database
-------------------------------
The first thing needed is a |Database| of all the packages and their dependencies.
Packages are organised in repositories, described by a
:cpp:type:`RepoInfo <mamba::solver::libsolv::RepoInfo>`.
This serves to resolve explicit channel requirements or channel priority.
As such, the database constructor takes a set of
:cpp:type:`ChannelResolveParams <mamba::specs::ChannelResolveParams>`
to work with :cpp:type:`Channel <mamba::specs::Channel>` data
internally (see :ref:`the usage section on Channels <libmamba_usage_channel>` for more
information).

The first way to add a repository is from a list of |PackageInfo| using
:cpp:func:`DataBase.add_repo_from_packages <mamba::solver::libsolv::Database::add_repo_from_packages>`:

.. code:: python

   import libmambapy

   mamba_context = libmambapy.Context()

   db = libmambapy.solver.libsolv.Database(
       libmambapy.specs.ChannelResolveParams(channel_alias="https://conda.anaconda.org")
   )

   repo1 = db.add_repo_from_packages(
       packages=[
           libmambapy.specs.PackageInfo(name="python", version="3.8", ...),
           libmambapy.specs.PackageInfo(name="pip", version="3.9", ...),
           ...,
       ],
       name="myrepo",
   )

The second way of loading packages is through Conda's repository index format ``repodata.json``
using
:cpp:func:`DataBase.add_repo_from_repodata <mamba::solver::libsolv::Database::add_repo_from_repodata>`.
This is meant for convenience, and is not a performant alternative to the former method, since these files
grow large.

.. code:: python

   repo2 = db.add_repo_from_repodata(
       path="path/to/repodata.json",
       url="htts://conda.anaconda.org/conda-forge/linux-64",
   )

One of the repositories can be set to have a special meaning of "installed repository".
It is used as a reference point in the solver to compute changes.
For instance, if a package is required but is already available in the installed repo, the solving
result will not mention it.
The function
:cpp:func:`DataBase.set_installed_repo <mamba::solver::libsolv::Database::set_installed_repo>` is
used for that purpose.

.. code:: python

   db.set_installed_repo(repo1)

Binary serialization of the database (Advanced)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The |Database| reporitories can be serialized in binary format for faster reloading.
To ensure integrity and freshness of the serialized file, metadata about the packages,
such as source url and
:cpp:type:`RepodataOrigin <mamba::solver::libsolv::RepodataOrigin>`, are stored inside the
file when calling
:cpp:func:`DataBase.native_serialize_repo <mamba::solver::libsolv::Database::native_serialize_repo>` .
Upon reading, similar parameters are expected as inputs to
:cpp:func:`DataBase.add_repo_from_native_serialization <mamba::solver::libsolv::Database::add_repo_from_native_serialization>`.
If they mismatch, the loading results in an error.

A typical wokflow first tries to load a repository from such binary cache, and then quietly
fallbacks to ``repodata.json`` on failure.

Creating a solving request
--------------------------
All jobs that need to be resolved are added as part of a |Request|.
This includes installing, updating, removing packages, as well as solving cutomization parameters.

.. code:: python

   Request = libmambapy.solver.Request
   MatchSpec = libmambapy.specs.MatchSpec

   request = Request(
       jobs=[
           Request.Install(MatchSpec.parse("python>=3.9")),
           Request.Update(MatchSpec.parse("numpy")),
           Request.Remove(MatchSpec.parse("pandas"), clean_dependencies=False),
       ],
       flags=Request.Flags(
           allow_downgrade=True,
           allow_uninstall=True,
       ),
   )

Solving the request
-------------------
The |Request| and the |Database| are the two input parameters needed to solve an environment.
This task is achieved with the :cpp:func:`Solver.solve <mamba::solver::libsolv::Solver::solve>`
method.

.. code:: python

   solver = libmambapy.solver.libsolv.Solver()
   outcome = solver.solve(db, request)

The outcome can be of two types, either a |Solution| listing packages (|Packageinfo|) and the
action to take on them (install, remove...), or an |UnSolvable| type when no solution exists
(because of conflict, missing packages...).

Examine the solution
~~~~~~~~~~~~~~~~~~~~
We can test if a valid solution exists by checking the type of the outcome.
The attribute :cpp:member:`Solution.actions <mamba::solver::Solution::actions>` contains the actions
to take on the installed repository so that it satisfies the |Request| requirements.

.. code:: python

    Solution = libmambapy.solver.Solution

    if isinstance(outcome, Solution):
        for action in outcome.actions:
            if isinstance(action, Solution.Upgrade):
                my_upgrade(from_pkg=action.remove, to_pkg=action.install)
            if isinstance(action, Solution.Reinstall):
                ...
            ...

Alternatively, an easy way to compute the update to the environment is to check for ``install`` and
``remove`` members, since they will populate the relevant fields for all actions:

.. code:: python

    Solution = libmambapy.solver.Solution

    if isinstance(outcome, Solution):
        for action in outcome.actions:
            if hasattr(action, "install"):
                my_download_and_install(action.install)
            # WARN: Do not use `elif` since actions like `Upgrade`
            # are represented as an `install` and `remove` pair.
            if hasattr(action, "remove"):
                my_delete(action.remove)

Understand unsolvable problems
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
When a problem has no |Solution|, it is inherenty hard to come up with an explanation.
In the easiest case, a required package is missing from the |Database|.
In the most complex, many package dependencies are incompatible without a single culprit.
In this case, packages should be rebuilt with weaker requirements, or with more build variants.
The |UnSolvable| class attempts to build an explanation.

The :cpp:func:`UnSolvable.problems <mamba::solver::libsolv::UnSolvable::problems>` is a list
of problems, as defined by the solver.
It is not easy to understand without linking it to specific |MatchSpec| and |PackageInfo|.
The method
:cpp:func:`UnSolvable.problems_graph <mamba::solver::libsolv::UnSolvable::problems_graph>`
gives a more structured graph of package dependencies and incompatibilities.
This graph is the underlying mechanism used in
:cpp:func:`UnSolvable.explain_problems <mamba::solver::libsolv::UnSolvable::explain_problems>`
to build a detail unsolvability message.
