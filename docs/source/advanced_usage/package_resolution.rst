Package resolution
==================

To resolve packages, mamba uses the `libsolv <https://github.com/openSUSE/libsolv>`_ library.
Libsolv employs a "backtracking" satisfiability (SAT) solver to make sure that each requested spec and the dependencies are properly satisfied.

It is possible to inspect the package resolution process by adding ``-vvv`` on the command line (which activates the triple-"verbose" mode). In this mode, curl and libsolv will print additional information.

.. note::
	This documentation talks about "track_feature". Track features are a "deprecated" conda feature that are still used solely to "down-weight" variant packages. Conda tries to "globally minimize" the amount of track_features in an environment. In mamba, we don't implement the same global optimization but we do de-prioritize track features. If you want to properly de-prioritize packages with mamba, please make sure to add the track_feature in the variant package or at least as a first-order dependency (e.g. numpy-1.20-pypy should either have a track feature or a specific dependency to ``python 3.8 *pypy`` which then in turn should have the track_feature).

The package resolution basically filters, sorts and selects packages until a working solution is found.

The pruning and sorting works as follows (implemented in ``policy.c`` in libsolv):

**solver_prune_to_highest_prio**

- if strict channel priority is activated, we prune (filter) packages not belonging to the highest prio channel
- if a solvable is installed, it is sorted to the front and preferred

**prune_to_best_version_conda**

- we first sort by track_feature: if a package has a track_feature, it's sorted to the bottom
- next we sort by version -- highest versions first (in libsolv, a version string is called ``evr`` which stands for epoch/version/release)
- then we sort by the sub-priority of the repository, preferring the repo with higher sub-priority
- next we compare the build number

Now, if multiple packages with the same build number are found, libsolv will create "variant" branches. The variants are also sorted, according to the following:

**sort_by_best_dependencies**

- first it's checked if the variant packages have different dependencies. For example, there can be one variant package that's built for ``python >=3.7,<3.8`` and one for ``python >=3.8,<3.9``.
- if we find different requirements, we check if one selection **only selects packages with a track_feature** (this would be the case with a match spec like ``python 3.8.* *pypy``). In this case, we de-prioritize that variant
- if there is no difference in track features in the dependencies, we want to prefer the variant for the higher dependency (python 3.8 in this case). To do this, we find all matching packages for ``python >=3.7,<3.8`` and ``python >=3.8,<3.9`` and check which of the two specs selects the higher version
- if we still find no difference, we compare the timestamps of the two packages and prefer the package that was added later to the repository


Examples
--------

**Simple example:**

To give some example orderings, we could look at the python package.
In conda-forge, we'd find something like

- python 3.9.2 HASH_cpython [build number: 1]
- python 3.9.2 HASH_cpython [build number: 0]
- python 3.9.1 HASH_cpython
- python 3.8 HASH_cpython
- python 3.7 HASH_cpython
- python 3.7 HASH_pypy [track_feature: pypy]

If we want to plainly install "python", we would prefer python 3.9.2 with build number 1 since it has the highest version and build number.

If we want to install ``python 3.7.*`` we would prefer ``python 3.7 HASH_cpython`` since it does not come with a track_feature.

**More difficult example:**

On conda-forge, we're building "variant packages" for numpy (and other packages requiring the C API of Python). This means for a given version of numpy, we'll end up with ~5 almost equivalent variant packages, for cpython 3.6, 3.7 and 3.8 as well as pypy 3.6 and 3.7.
For this example the default is the cpython build of numpy. However, currently conda-forge does not apply the down-weigthing via track_feature on the terminal node (numpy), but only in some dependency package (such as python).

For the case where we want to simply install ``numpy``, we need to find which numpy variant installs the highest python package. In this case libsolv would decide for ``numpy-1.20-cpython38``.

If we install ``numpy python=3.7`` we have two potential variants: ``numpy-1.20-cpython37`` and ``numpy-1.20-pypy37``. In this case we need to inspect whether one of those two builds will require exclusively packages with a track_feature applied. And indeed, the ``pypy37`` package will have a requirement on ``python_abi 3.7 *pypy`` and **all** packages matching this requirement have a track_feature, so that it will be down-weighted.
