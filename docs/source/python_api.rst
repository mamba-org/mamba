=======================
Python API of ``mamba``
=======================

Using internal mamba APIs
-------------------------

The core of ``mamba`` is written in C++, but we expose the internals of mamba with a Python API
(using ``pybind11``).

You can import ``libmambapy`` containing the Python API using ``import libmambapy`` without having ``mamba`` installed.

These bindings expose the following objects (`boa`_ has a full example):

- ``Context``: a singleton configuration object. All global configuration goes through this. From Python you can use the context object like so:

.. code:: python

   import libmambapy

   ctx = libmambapy.Context.instance()
   ctx.conda_prefix = "/home/wolfv/conda"
   print(ctx.root_prefix)


Here is an example usage of ``libmambapy``:

.. code:: python

    def get_index(
        channel_urls=(),
        prepend=True,
        platform=None,
        use_local=False,
        use_cache=False,
        unknown=None,
        prefix=None,
        repodata_fn="repodata.json",
    ):
        check_allowlist(channel_urls)

        dlist = libmambapy.DownloadTargetList()

        index = []
        for idx, url in enumerate(channel_urls):
            channel = Channel(url)

            full_url = channel.url(with_credentials=True) + "/" + repodata_fn
            full_path_cache = os.path.join(
                create_cache_dir(), cache_fn_url(full_url, repodata_fn)
            )

            # Channels might not have a name.
            if channel.name is None:
                name_and_subdir = channel.subdir
            else:
                name_and_subdir = channel.name + "/" + channel.subdir
            sd = libmambapy.SubdirData(name_and_subdir, full_url, full_path_cache)

            index.append((sd, channel))
            dlist.add(sd)

        is_downloaded = dlist.download(libmambapy.MAMBA_DOWNLOAD_FAILFAST)

        if not is_downloaded:
            raise RuntimeError("Error downloading repodata.")

        return index


    class MambaSolver:
        def __init__(self, prefix, channels, platform):

            api_ctx = libmambapy.Context()
            api_ctx.conda_prefix = prefix

            self.channels = channels
            self.platform = platform
            self.index = get_index(channels, platform=platform)
            self.local_index = []
            self.pool = libmambapy.Pool()
            self.repos = []

            start_prio = len(channels)
            priority = start_prio
            subpriority = 0  # wrong! :)
            for subdir, channel in self.index:
                repo = libmambapy.Repo(
                    self.pool,
                    str(channel),
                    subdir.cache_path(),
                    channel.url(with_credentials=True),
                )
                repo.set_priority(start_prio, subpriority)
                start_prio -= 1
                self.repos.append(repo)

            self.local_repos = {}

        def solve(self, specs, prefix):
            """Solve given a set of specs.
            Parameters
            ----------
            specs : list of str
                A list of package specs. You can use `conda.models.match_spec.MatchSpec`
                to get them to the right form by calling
                `MatchSpec(mypec).conda_build_form()`
            Returns
            -------
            solvable : bool
                True if the set of specs has a solution, False otherwise.
            """
            solver_options = [(libmambapy.SOLVER_FLAG_ALLOW_DOWNGRADE, 1)]
            api_solver = libmambapy.Solver(self.pool, solver_options)
            _specs = specs

            api_solver.add_jobs(_specs, libmambapy.SOLVER_INSTALL)
            success = api_solver.try_solve()

            if not success:
                error_string = "Mamba failed to solve:\n"
                for s in _specs:
                    error_string += f" - {s}\n"
                error_string += "\nwith channels:\n"
                for c in self.channels:
                    error_string += f" - {c}\n"
                pstring = api_solver.problems_to_str()
                pstring = "\n".join(["   " + l for l in pstring.split("\n")])
                error_string += f"\nThe reported errors are:\n⇟{pstring}"
                print(error_string)
                exit(1)

            package_cache = libmambapy.MultiPackageCache(pkgs_dirs)

            t = libmambapy.Transaction(api_solver, package_cache)
            return t

.. _boa: https://www.github.com/mamba-org/boa/blob/main/boa/core/solver.py
