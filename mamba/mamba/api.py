import os
import pathlib
import sys

import libmambapy

from .utils import get_index, load_channels

__all__ = ["MambaSolver", "create", "install"]


class MambaSolver:
    def __init__(self, channels, platform, context, output_folder=None):
        self.channels = channels
        self.platform = platform
        self.context = context
        self.output_folder = output_folder or "local"
        self.pool = libmambapy.Pool()
        self.repos = []
        self.index = load_channels(
            self.pool, self.channels, self.repos, platform=platform
        )

        self.local_index = []
        self.local_repos = {}
        # load local repo, too
        self.replace_channels()

    def replace_installed(self, prefix):
        prefix_data = libmambapy.PrefixData(prefix)
        vp = libmambapy.get_virtual_packages()
        prefix_data.add_packages(vp)
        repo = libmambapy.Repo(self.pool, prefix_data)
        repo.set_installed()

    def replace_channels(self):
        self.local_index = get_index(
            (self.output_folder,), platform=self.platform, prepend=False
        )

        for _, v in self.local_repos.items():
            v.clear(True)

        start_prio = len(self.channels) + len(self.index)
        for subdir, channel in self.local_index:
            if not subdir.loaded():
                continue

            # support new mamba
            if isinstance(channel, dict):
                channelstr = channel["url"]
                channelurl = channel["url"]
            else:
                channelstr = str(channel)
                channelurl = channel.url(with_credentials=True)

            cp = subdir.cache_path()
            if cp.endswith(".solv"):
                os.remove(subdir.cache_path())
                cp = cp.replace(".solv", ".json")

            self.local_repos[channelstr] = libmambapy.Repo(
                self.pool, channelstr, cp, channelurl
            )

            self.local_repos[channelstr].set_priority(start_prio, 0)
            start_prio -= 1

    def solve(self, specs, pkg_cache_path=None):
        """Solve given a set of specs.
        Parameters
        ----------
        specs : list of str
            A list of package specs. You can use `conda.models.match_spec.MatchSpec`
            to get them to the right form by calling
            `MatchSpec(mypec).conda_build_form()`
        Returns
        -------
        transaction : libmambapy.Transaction
            The mamba transaction.
        Raises
        ------
        RuntimeError :
            If the solver did not find a solution.
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
            error_string += api_solver.explain_problems()
            print(error_string)
            raise RuntimeError("Solver could not find solution." + error_string)

        if pkg_cache_path is None:
            # use values from conda
            pkg_cache_path = self.context.pkgs_dirs

        package_cache = libmambapy.MultiPackageCache(pkg_cache_path)
        return libmambapy.Transaction(api_solver, package_cache)


def install(
    env_name: str,
    specs: tuple = (),
    channels: tuple = (),
    target_platform: str = None,
    base_prefix: str = None,
):
    """Install packages in a given environment.

    Arguments
    ---------
    env_name : str
        The name of the environment where to install the packages.
    specs : tuple of str
        The list of spec strings e.g. ['xeus-python', 'matplotlib=3'].
    channels : tuple of str
        The channels from which to pull packages e.g. ['default', 'conda-forge'].
    base_prefix : str
        The base prefix where to create the environment, defaults to the current base prefix.

    Raises
    ------
    RuntimeError :
        If the solver did not find a solution or if the installation failed.
    """
    if base_prefix is None:
        base_prefix = os.environ.get("MAMBA_ROOT_PREFIX", sys.prefix)

    base_prefix = pathlib.Path(base_prefix)

    prefix = base_prefix / "envs" / env_name
    (prefix / "conda-meta").mkdir(parents=True, exist_ok=True)
    (base_prefix / "pkgs").mkdir(parents=True, exist_ok=True)

    context = libmambapy.Context()
    context.prefix_params.target_prefix = str(prefix)
    context.pkgs_dirs = [str(base_prefix / "pkgs")]

    solver = MambaSolver(channels, target_platform, context)

    transaction = solver.solve(specs)

    return transaction.execute(libmambapy.PrefixData(str(prefix)))


def create(
    env_name: str,
    specs: tuple = (),
    channels: tuple = (),
    target_platform: str = None,
    base_prefix: str = None,
):
    """Create a mamba environment.

    Arguments
    ---------
    env_name : str
        The name of the environment.
    specs : tuple of str
        The list of spec strings e.g. ['xeus-python', 'matplotlib=3'].
    channels : tuple of str
        The channels from which to pull packages e.g. ['default', 'conda-forge'].
    target_platform : str
        The target platform for the environment.
    base_prefix : str
        The base prefix where to create the environment, defaults to the current base prefix.

    Raises
    ------
    RuntimeError :
        If the solver did not find a solution or if the installation failed.
    """
    return install(env_name, specs, channels, target_platform, base_prefix)
