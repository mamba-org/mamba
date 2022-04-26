=======================
Python API of ``mamba``
=======================

The core of ``mamba`` is written in C++, but we expose the internals of mamba with a Python API (using ``pybind11``).

You can import the ``mamba_api`` using ``from mamba import mamba_api``.

The mamba_api exposes the following objects:

- ``Context``: a singleton configuration object. All global configuration goes through this. From Python you can use the context object like so:

.. code::

   from mamba import mamba_api
   mamba_api.Context().conda_prefix = "/home/wolfv/conda"
   ctx = mamba_api.Context()
   print(ctx.root_prefix)


Here is an example usage of the mamba_api:

.. code::

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
        check_whitelist(channel_urls)

        dlist = mamba_api.DownloadTargetList()

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
            sd = mamba_api.SubdirData(name_and_subdir, full_url, full_path_cache)

            index.append((sd, channel))
            dlist.add(sd)

        is_downloaded = dlist.download(mamba_api.MAMBA_DOWNLOAD_FAILFAST)

        if not is_downloaded:
            raise RuntimeError("Error downloading repodata.")

        return index

    class MambaSolver:
        def __init__(self, prefix, channels, platform):

            api_ctx = mamba_api.Context()
            api_ctx.conda_prefix = prefix

            self.channels = channels
            self.platform = platform
            self.index = get_index(channels, platform=platform)
            self.local_index = []
            self.pool = mamba_api.Pool()
            self.repos = []

            start_prio = len(channels)
            priority = start_prio
            subpriority = 0  # wrong! :)
            for subdir, channel in self.index:
                repo = mamba_api.Repo(
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
            solver_options = [(mamba_api.SOLVER_FLAG_ALLOW_DOWNGRADE, 1)]
            api_solver = mamba_api.Solver(self.pool, solver_options)
            _specs = specs

            api_solver.add_jobs(_specs, mamba_api.SOLVER_INSTALL)
            success = api_solver.solve()

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

            package_cache = mamba_api.MultiPackageCache(pkgs_dirs)

            t = mamba_api.Transaction(api_solver, package_cache)
            return t


Let's walk through this example:

We first use the ``get_index`` method to download repository data from the channels.
