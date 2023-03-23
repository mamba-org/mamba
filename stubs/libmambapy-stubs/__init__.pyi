from __future__ import annotations
import libmambapy
import typing
from libmambapy.bindings import Channel
from libmambapy.bindings import ChannelPriority
from libmambapy.bindings import CompressedProblemsGraph
from libmambapy.bindings import Configuration
from libmambapy.bindings import Context
from libmambapy.bindings import DownloadTargetList
from libmambapy.bindings import ExtraPkgInfo
from libmambapy.bindings import History
from libmambapy.bindings import Key
from libmambapy.bindings import KeyMgr
from libmambapy.bindings import LockFile
from libmambapy.bindings import LogLevel
from libmambapy.bindings import MambaNativeException
from libmambapy.bindings import MatchSpec
from libmambapy.bindings import MultiPackageCache
from libmambapy.bindings import PackageInfo
from libmambapy.bindings import Path
from libmambapy.bindings import PkgMgr
from libmambapy.bindings import Pool
from libmambapy.bindings import PrefixData
from libmambapy.bindings import ProblemsGraph
from libmambapy.bindings import Query
from libmambapy.bindings import QueryFormat
from libmambapy.bindings import Repo
from libmambapy.bindings import RoleBase
from libmambapy.bindings import RoleBaseExtension
from libmambapy.bindings import RoleFullKeys
from libmambapy.bindings import RootImpl
from libmambapy.bindings import RootRole
from libmambapy.bindings import Solver
from libmambapy.bindings import SolverProblem
from libmambapy.bindings import SolverRuleinfo
from libmambapy.bindings import SpecBase
from libmambapy.bindings import SpecImpl
from libmambapy.bindings import SubdirData
from libmambapy.bindings import Transaction
from libmambapy.bindings import ostream_redirect
import os

__all__ = [
    "Channel",
    "ChannelPriority",
    "CompressedProblemsGraph",
    "Configuration",
    "Context",
    "DownloadTargetList",
    "ExtraPkgInfo",
    "History",
    "Key",
    "KeyMgr",
    "LockFile",
    "LogLevel",
    "MAMBA_CLEAN_ALL",
    "MAMBA_CLEAN_INDEX",
    "MAMBA_CLEAN_LOCKS",
    "MAMBA_CLEAN_PKGS",
    "MAMBA_CLEAN_TARBALLS",
    "MAMBA_DOWNLOAD_FAILFAST",
    "MAMBA_DOWNLOAD_SORT",
    "MAMBA_FORCE_REINSTALL",
    "MAMBA_NO_DEPS",
    "MAMBA_ONLY_DEPS",
    "MambaNativeException",
    "MatchSpec",
    "MultiPackageCache",
    "PackageInfo",
    "Path",
    "PkgMgr",
    "Pool",
    "PrefixData",
    "ProblemsGraph",
    "Query",
    "QueryFormat",
    "Repo",
    "RoleBase",
    "RoleBaseExtension",
    "RoleFullKeys",
    "RootImpl",
    "RootRole",
    "SOLVER_ALLOWUNINSTALL",
    "SOLVER_CLEANDEPS",
    "SOLVER_DISFAVOR",
    "SOLVER_DISTUPGRADE",
    "SOLVER_DROP_ORPHANED",
    "SOLVER_ERASE",
    "SOLVER_ESSENTIAL",
    "SOLVER_FAVOR",
    "SOLVER_FLAG_ADD_ALREADY_RECOMMENDED",
    "SOLVER_FLAG_ALLOW_ARCHCHANGE",
    "SOLVER_FLAG_ALLOW_DOWNGRADE",
    "SOLVER_FLAG_ALLOW_NAMECHANGE",
    "SOLVER_FLAG_ALLOW_UNINSTALL",
    "SOLVER_FLAG_ALLOW_VENDORCHANGE",
    "SOLVER_FLAG_BEST_OBEY_POLICY",
    "SOLVER_FLAG_BREAK_ORPHANS",
    "SOLVER_FLAG_DUP_ALLOW_ARCHCHANGE",
    "SOLVER_FLAG_DUP_ALLOW_DOWNGRADE",
    "SOLVER_FLAG_DUP_ALLOW_NAMECHANGE",
    "SOLVER_FLAG_DUP_ALLOW_VENDORCHANGE",
    "SOLVER_FLAG_FOCUS_BEST",
    "SOLVER_FLAG_FOCUS_INSTALLED",
    "SOLVER_FLAG_IGNORE_RECOMMENDED",
    "SOLVER_FLAG_INSTALL_ALSO_UPDATES",
    "SOLVER_FLAG_KEEP_EXPLICIT_OBSOLETES",
    "SOLVER_FLAG_KEEP_ORPHANS",
    "SOLVER_FLAG_NEED_UPDATEPROVIDE",
    "SOLVER_FLAG_NO_AUTOTARGET",
    "SOLVER_FLAG_NO_INFARCHCHECK",
    "SOLVER_FLAG_NO_UPDATEPROVIDE",
    "SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED",
    "SOLVER_FLAG_SPLITPROVIDES",
    "SOLVER_FLAG_STRICT_REPO_PRIORITY",
    "SOLVER_FLAG_STRONG_RECOMMENDS",
    "SOLVER_FLAG_URPM_REORDER",
    "SOLVER_FLAG_YUM_OBSOLETES",
    "SOLVER_FORCEBEST",
    "SOLVER_INSTALL",
    "SOLVER_JOBMASK",
    "SOLVER_LOCK",
    "SOLVER_MULTIVERSION",
    "SOLVER_NOAUTOSET",
    "SOLVER_NOOP",
    "SOLVER_NOTBYUSER",
    "SOLVER_ORUPDATE",
    "SOLVER_SELECTMASK",
    "SOLVER_SETARCH",
    "SOLVER_SETEV",
    "SOLVER_SETEVR",
    "SOLVER_SETMASK",
    "SOLVER_SETNAME",
    "SOLVER_SETREPO",
    "SOLVER_SETVENDOR",
    "SOLVER_SOLVABLE",
    "SOLVER_SOLVABLE_ALL",
    "SOLVER_SOLVABLE_NAME",
    "SOLVER_SOLVABLE_ONE_OF",
    "SOLVER_SOLVABLE_PROVIDES",
    "SOLVER_SOLVABLE_REPO",
    "SOLVER_TARGETED",
    "SOLVER_UPDATE",
    "SOLVER_USERINSTALLED",
    "SOLVER_VERIFY",
    "SOLVER_WEAK",
    "SOLVER_WEAKENDEPS",
    "Solver",
    "SolverProblem",
    "SolverRuleinfo",
    "SpecBase",
    "SpecImpl",
    "SubdirData",
    "Transaction",
    "bindings",
    "cache_fn_url",
    "cancel_json_output",
    "clean",
    "create_cache_dir",
    "generate_ed25519_keypair",
    "get_channels",
    "get_virtual_packages",
    "os",
    "ostream_redirect",
    "sign",
    "simplify_conflicts",
    "transmute"
]


def cache_fn_url(arg0: str) -> str:
    pass
def cancel_json_output() -> None:
    pass
def clean(arg0: int) -> None:
    pass
def create_cache_dir(arg0: bindings.Path) -> str:
    pass
def generate_ed25519_keypair() -> typing.Tuple[str, str]:
    pass
def get_channels(arg0: typing.List[str]) -> typing.List[bindings.Channel]:
    pass
def get_virtual_packages() -> typing.List[bindings.PackageInfo]:
    pass
def sign(data: str, secret_key: str) -> str:
    pass
def simplify_conflicts(arg0: bindings.ProblemsGraph) -> bindings.ProblemsGraph:
    pass
def transmute(source_package: bindings.Path, destination_package: bindings.Path, compression_level: int, compression_threads: int = 1) -> bool:
    pass
MAMBA_CLEAN_ALL = 1
MAMBA_CLEAN_INDEX = 2
MAMBA_CLEAN_LOCKS = 16
MAMBA_CLEAN_PKGS = 4
MAMBA_CLEAN_TARBALLS = 8
MAMBA_DOWNLOAD_FAILFAST = 1
MAMBA_DOWNLOAD_SORT = 2
MAMBA_FORCE_REINSTALL = 4
MAMBA_NO_DEPS = 1
MAMBA_ONLY_DEPS = 2
SOLVER_ALLOWUNINSTALL = 2816
SOLVER_CLEANDEPS = 262144
SOLVER_DISFAVOR = 3328
SOLVER_DISTUPGRADE = 1792
SOLVER_DROP_ORPHANED = 2304
SOLVER_ERASE = 512
SOLVER_ESSENTIAL = 131072
SOLVER_FAVOR = 3072
SOLVER_FLAG_ADD_ALREADY_RECOMMENDED = 8
SOLVER_FLAG_ALLOW_ARCHCHANGE = 2
SOLVER_FLAG_ALLOW_DOWNGRADE = 1
SOLVER_FLAG_ALLOW_NAMECHANGE = 10
SOLVER_FLAG_ALLOW_UNINSTALL = 4
SOLVER_FLAG_ALLOW_VENDORCHANGE = 3
SOLVER_FLAG_BEST_OBEY_POLICY = 12
SOLVER_FLAG_BREAK_ORPHANS = 19
SOLVER_FLAG_DUP_ALLOW_ARCHCHANGE = 15
SOLVER_FLAG_DUP_ALLOW_DOWNGRADE = 14
SOLVER_FLAG_DUP_ALLOW_NAMECHANGE = 17
SOLVER_FLAG_DUP_ALLOW_VENDORCHANGE = 16
SOLVER_FLAG_FOCUS_BEST = 24
SOLVER_FLAG_FOCUS_INSTALLED = 20
SOLVER_FLAG_IGNORE_RECOMMENDED = 7
SOLVER_FLAG_INSTALL_ALSO_UPDATES = 26
SOLVER_FLAG_KEEP_EXPLICIT_OBSOLETES = 11
SOLVER_FLAG_KEEP_ORPHANS = 18
SOLVER_FLAG_NEED_UPDATEPROVIDE = 22
SOLVER_FLAG_NO_AUTOTARGET = 13
SOLVER_FLAG_NO_INFARCHCHECK = 9
SOLVER_FLAG_NO_UPDATEPROVIDE = 5
SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED = 27
SOLVER_FLAG_SPLITPROVIDES = 6
SOLVER_FLAG_STRICT_REPO_PRIORITY = 28
SOLVER_FLAG_STRONG_RECOMMENDS = 25
SOLVER_FLAG_URPM_REORDER = 23
SOLVER_FLAG_YUM_OBSOLETES = 21
SOLVER_FORCEBEST = 1048576
SOLVER_INSTALL = 256
SOLVER_JOBMASK = 65280
SOLVER_LOCK = 1536
SOLVER_MULTIVERSION = 1280
SOLVER_NOAUTOSET = 536870912
SOLVER_NOOP = 0
SOLVER_NOTBYUSER = 4194304
SOLVER_ORUPDATE = 524288
SOLVER_SELECTMASK = 255
SOLVER_SETARCH = 67108864
SOLVER_SETEV = 16777216
SOLVER_SETEVR = 33554432
SOLVER_SETMASK = 2130706432
SOLVER_SETNAME = 1073741824
SOLVER_SETREPO = 268435456
SOLVER_SETVENDOR = 134217728
SOLVER_SOLVABLE = 1
SOLVER_SOLVABLE_ALL = 6
SOLVER_SOLVABLE_NAME = 2
SOLVER_SOLVABLE_ONE_OF = 4
SOLVER_SOLVABLE_PROVIDES = 3
SOLVER_SOLVABLE_REPO = 5
SOLVER_TARGETED = 2097152
SOLVER_UPDATE = 768
SOLVER_USERINSTALLED = 2560
SOLVER_VERIFY = 2048
SOLVER_WEAK = 65536
SOLVER_WEAKENDEPS = 1024
