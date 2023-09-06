from __future__ import annotations
import typing

__all__ = [
    "Channel",
    "ChannelPriority",
    "CompressedProblemsGraph",
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
    "TimeRef",
    "Transaction",
    "cache_fn_url",
    "cancel_json_output",
    "clean",
    "create_cache_dir",
    "generate_ed25519_keypair",
    "get_channels",
    "get_virtual_packages",
    "init_console",
    "ostream_redirect",
    "sign",
    "simplify_conflicts",
    "transmute",
]

class Channel:
    def __init__(self, arg0: str) -> None: ...
    def __repr__(self) -> str: ...
    def platform_url(self, platform: str, with_credentials: bool = ...) -> str: ...
    def platform_urls(self, with_credentials: bool = ...) -> list[tuple[str, str]]: ...
    def urls(self, with_credentials: bool = ...) -> list[str]: ...
    @property
    def auth(self) -> str | None: ...
    @property
    def canonical_name(self) -> str: ...
    @property
    def location(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def package_filename(self) -> str | None: ...
    @property
    def platforms(self) -> list[str]: ...
    @property
    def scheme(self) -> str: ...
    @property
    def token(self) -> str | None: ...

class ChannelPriority:
    """
    Members:

      kFlexible

      kStrict

      kDisabled
    """

    __members__: typing.ClassVar[
        dict[str, ChannelPriority]
    ]  # value = {'kFlexible': <ChannelPriority.kFlexible: 1>, 'kStrict': <ChannelPriority.kStrict: 2>, 'kDisabled': <ChannelPriority.kDisabled: 0>}
    kDisabled: typing.ClassVar[
        ChannelPriority
    ]  # value = <ChannelPriority.kDisabled: 0>
    kFlexible: typing.ClassVar[
        ChannelPriority
    ]  # value = <ChannelPriority.kFlexible: 1>
    kStrict: typing.ClassVar[ChannelPriority]  # value = <ChannelPriority.kStrict: 2>
    def __eq__(self, other: typing.Any) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: int) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: typing.Any) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: int) -> None: ...
    def __str__(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class CompressedProblemsGraph:
    class ConflictMap:
        def __bool__(self) -> bool: ...
        def __contains__(self, arg0: int) -> bool: ...
        def __init__(self) -> None: ...
        def __iter__(self) -> typing.Iterator: ...
        def __len__(self) -> int: ...
        def add(self, arg0: int, arg1: int) -> bool: ...
        def clear(self) -> None: ...
        def conflicts(self, arg0: int) -> set[int]: ...
        def has_conflict(self, arg0: int) -> bool: ...
        def in_conflict(self, arg0: int, arg1: int) -> bool: ...

    class ConstraintListNode:
        def __bool__(self) -> bool: ...
        def __init__(self) -> None: ...
        def __iter__(self) -> typing.Iterator: ...
        def __len__(self) -> int: ...
        def add(self, arg0: ProblemsGraph.ConstraintNode) -> None: ...
        def build_strings_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...
        def clear(self) -> None: ...
        def name(self) -> str: ...
        def versions_and_build_strings_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...
        def versions_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...

    class DependencyList:
        def __bool__(self) -> bool: ...
        def __init__(self) -> None: ...
        def __iter__(self) -> typing.Iterator: ...
        def __len__(self) -> int: ...
        def add(self, arg0: MatchSpec) -> None: ...
        def build_strings_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...
        def clear(self) -> None: ...
        def name(self) -> str: ...
        def versions_and_build_strings_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...
        def versions_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...

    class PackageListNode:
        def __bool__(self) -> bool: ...
        def __init__(self) -> None: ...
        def __iter__(self) -> typing.Iterator: ...
        def __len__(self) -> int: ...
        def add(self, arg0: ProblemsGraph.PackageNode) -> None: ...
        def build_strings_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...
        def clear(self) -> None: ...
        def name(self) -> str: ...
        def versions_and_build_strings_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...
        def versions_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...

    class RootNode:
        def __init__(self) -> None: ...

    class UnresolvedDependencyListNode:
        def __bool__(self) -> bool: ...
        def __init__(self) -> None: ...
        def __iter__(self) -> typing.Iterator: ...
        def __len__(self) -> int: ...
        def add(self, arg0: ProblemsGraph.UnresolvedDependencyNode) -> None: ...
        def build_strings_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...
        def clear(self) -> None: ...
        def name(self) -> str: ...
        def versions_and_build_strings_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...
        def versions_trunc(
            self,
            sep: str = ...,
            etc: str = ...,
            threshold: int = ...,
            remove_duplicates: bool = ...,
        ) -> tuple[str, int]: ...

    @staticmethod
    @typing.overload
    def from_problems_graph(
        arg0: ProblemsGraph,
        arg1: typing.Callable[[libmambapy.bindings.ProblemsGraph, int, int], bool],
    ) -> CompressedProblemsGraph: ...
    @staticmethod
    @typing.overload
    def from_problems_graph(arg0: ProblemsGraph) -> CompressedProblemsGraph: ...
    def conflicts(self) -> ProblemsGraph.ConflictMap: ...
    def graph(
        self,
    ) -> tuple[
        dict[
            int,
            ProblemsGraph.RootNode
            | CompressedProblemsGraph.PackageListNode
            | CompressedProblemsGraph.UnresolvedDependencyListNode
            | CompressedProblemsGraph.ConstraintListNode,
        ],
        dict[tuple[int, int], CompressedProblemsGraph.DependencyList],
    ]: ...
    def root_node(self) -> int: ...
    def tree_message(self) -> str: ...

class Context:
    class OutputParams:
        json: bool
        quiet: bool
        verbosity: int
        def __init__(self) -> None: ...

    class PrefixParams:
        conda_prefix: Path
        root_prefix: Path
        target_prefix: Path
        def __init__(self) -> None: ...

    class RemoteFetchParams:
        connect_timeout_secs: int
        max_retries: int
        proxy_servers: dict[str, str]
        retry_backoff: int
        retry_timeout: int
        ssl_verify: str
        user_agent: str
        def __init__(self) -> None: ...

    class ThreadsParams:
        download_threads: int
        extract_threads: int
        def __init__(self) -> None: ...
    add_pip_as_python_dependency: bool
    always_yes: bool
    channel_alias: str
    channel_priority: ChannelPriority
    channels: list[str]
    conda_prefix: Path
    connect_timeout_secs: int
    custom_channels: dict[str, str]
    custom_multichannels: dict[str, list[str]]
    default_channels: list[str]
    download_only: bool
    download_threads: int
    dry_run: bool
    envs_dirs: list[Path]
    experimental_sat_error_message: bool
    extract_threads: int
    json: bool
    local_repodata_ttl: int
    max_retries: int
    offline: bool
    output_params: Context.OutputParams
    pkgs_dirs: list[Path]
    platform: str
    prefix_params: Context.PrefixParams
    proxy_servers: dict[str, str]
    quiet: bool
    remote_fetch_params: Context.RemoteFetchParams
    retry_backoff: int
    retry_timeout: int
    root_prefix: Path
    ssl_verify: str
    target_prefix: Path
    threads_params: Context.ThreadsParams
    use_index_cache: bool
    use_lockfiles: bool
    use_only_tar_bz2: bool
    user_agent: str
    verbosity: int
    def __init__(self) -> None: ...
    def set_log_level(self, arg0: LogLevel) -> None: ...
    def set_verbosity(self, arg0: int) -> None: ...

class DownloadTargetList:
    def __init__(self) -> None: ...
    def add(self, arg0: SubdirData) -> None: ...
    def download(self, arg0: int) -> bool: ...

class ExtraPkgInfo:
    noarch: str
    repo_url: str
    def __init__(self) -> None: ...

class History:
    def __init__(self, arg0: Path) -> None: ...
    def get_requested_specs_map(self) -> dict[str, MatchSpec]: ...

class Key:
    keytype: str
    keyval: str
    scheme: str
    @staticmethod
    def from_ed25519(arg0: str) -> Key: ...
    @property
    def json_str(self) -> str: ...

class KeyMgr(RoleBase, RoleBaseExtension):
    def __init__(self, arg0: str, arg1: RoleFullKeys, arg2: SpecBase) -> None: ...

class LockFile:
    def __init__(self, arg0: Path) -> None: ...

class LogLevel:
    """
    Members:

      TRACE

      DEBUG

      INFO

      WARNING

      ERROR

      CRITICAL

      OFF
    """

    CRITICAL: typing.ClassVar[LogLevel]  # value = <LogLevel.CRITICAL: 5>
    DEBUG: typing.ClassVar[LogLevel]  # value = <LogLevel.DEBUG: 1>
    ERROR: typing.ClassVar[LogLevel]  # value = <LogLevel.ERROR: 4>
    INFO: typing.ClassVar[LogLevel]  # value = <LogLevel.INFO: 2>
    OFF: typing.ClassVar[LogLevel]  # value = <LogLevel.OFF: 6>
    TRACE: typing.ClassVar[LogLevel]  # value = <LogLevel.TRACE: 0>
    WARNING: typing.ClassVar[LogLevel]  # value = <LogLevel.WARNING: 3>
    __members__: typing.ClassVar[
        dict[str, LogLevel]
    ]  # value = {'TRACE': <LogLevel.TRACE: 0>, 'DEBUG': <LogLevel.DEBUG: 1>, 'INFO': <LogLevel.INFO: 2>, 'WARNING': <LogLevel.WARNING: 3>, 'ERROR': <LogLevel.ERROR: 4>, 'CRITICAL': <LogLevel.CRITICAL: 5>, 'OFF': <LogLevel.OFF: 6>}
    def __eq__(self, other: typing.Any) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: int) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: typing.Any) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: int) -> None: ...
    def __str__(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class MambaNativeException(Exception):
    pass

class MatchSpec:
    @typing.overload
    def __init__(self) -> None: ...
    @typing.overload
    def __init__(self, arg0: str) -> None: ...
    def conda_build_form(self) -> str: ...

class MultiPackageCache:
    def __init__(self, arg0: list[Path]) -> None: ...
    def get_tarball_path(self, arg0: PackageInfo, arg1: bool) -> Path: ...
    @property
    def first_writable_path(self) -> Path: ...

class PackageInfo:
    build_number: int
    build_string: str
    channel: str
    constrains: list[str]
    defaulted_keys: set[str]
    depends: list[str]
    fn: str
    license: str
    md5: str
    name: str
    noarch: str
    sha256: str
    signatures: str
    size: int
    subdir: str
    timestamp: int
    track_features: str
    url: str
    version: str
    @typing.overload
    def __init__(self, name: str) -> None: ...
    @typing.overload
    def __init__(
        self, name: str, version: str, build_string: str, build_number: int
    ) -> None: ...

class Path:
    def __init__(self, arg0: str) -> None: ...
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...

class PkgMgr(RoleBase, RoleBaseExtension):
    def __init__(self, arg0: str, arg1: RoleFullKeys, arg2: SpecBase) -> None: ...

class Pool:
    def __init__(self) -> None: ...
    def create_whatprovides(self) -> None: ...
    def id2pkginfo(self, id: int) -> PackageInfo | None: ...
    @typing.overload
    def matchspec2id(self, ms: MatchSpec) -> int: ...
    @typing.overload
    def matchspec2id(self, ms: str) -> int: ...
    def select_solvables(self, id: int, sorted: bool = ...) -> list[int]: ...
    def set_debuglevel(self) -> None: ...

class PrefixData:
    def __init__(self, arg0: Path) -> None: ...
    def add_packages(self, arg0: list[PackageInfo]) -> None: ...
    @property
    def package_records(self) -> dict[str, PackageInfo]: ...

class ProblemsGraph:
    class ConflictMap:
        def __bool__(self) -> bool: ...
        def __contains__(self, arg0: int) -> bool: ...
        def __init__(self) -> None: ...
        def __iter__(self) -> typing.Iterator: ...
        def __len__(self) -> int: ...
        def add(self, arg0: int, arg1: int) -> bool: ...
        def clear(self) -> None: ...
        def conflicts(self, arg0: int) -> set[int]: ...
        def has_conflict(self, arg0: int) -> bool: ...
        def in_conflict(self, arg0: int, arg1: int) -> bool: ...

    class ConstraintNode(MatchSpec):
        pass

    class PackageNode(PackageInfo):
        pass

    class RootNode:
        def __init__(self) -> None: ...

    class UnresolvedDependencyNode(MatchSpec):
        pass
    @staticmethod
    def from_solver(arg0: Solver, arg1: Pool) -> ProblemsGraph: ...
    def conflicts(self) -> ProblemsGraph.ConflictMap: ...
    def graph(
        self,
    ) -> tuple[
        dict[
            int,
            ProblemsGraph.RootNode
            | ProblemsGraph.PackageNode
            | ProblemsGraph.UnresolvedDependencyNode
            | ProblemsGraph.ConstraintNode,
        ],
        dict[tuple[int, int], MatchSpec],
    ]: ...
    def root_node(self) -> int: ...

class Query:
    def __init__(self, arg0: Pool) -> None: ...
    def depends(self, arg0: str, arg1: QueryFormat) -> str: ...
    def find(self, arg0: str, arg1: QueryFormat) -> str: ...
    def whoneeds(self, arg0: str, arg1: QueryFormat) -> str: ...

class QueryFormat:
    """
    Members:

      JSON

      TREE

      TABLE

      PRETTY

      RECURSIVETABLE
    """

    JSON: typing.ClassVar[QueryFormat]  # value = <QueryFormat.JSON: 0>
    PRETTY: typing.ClassVar[QueryFormat]  # value = <QueryFormat.PRETTY: 3>
    RECURSIVETABLE: typing.ClassVar[
        QueryFormat
    ]  # value = <QueryFormat.RECURSIVETABLE: 4>
    TABLE: typing.ClassVar[QueryFormat]  # value = <QueryFormat.TABLE: 2>
    TREE: typing.ClassVar[QueryFormat]  # value = <QueryFormat.TREE: 1>
    __members__: typing.ClassVar[
        dict[str, QueryFormat]
    ]  # value = {'JSON': <QueryFormat.JSON: 0>, 'TREE': <QueryFormat.TREE: 1>, 'TABLE': <QueryFormat.TABLE: 2>, 'PRETTY': <QueryFormat.PRETTY: 3>, 'RECURSIVETABLE': <QueryFormat.RECURSIVETABLE: 4>}
    def __eq__(self, other: typing.Any) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: int) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: typing.Any) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: int) -> None: ...
    def __str__(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class Repo:
    @typing.overload
    def __init__(self, arg0: Pool, arg1: str, arg2: str, arg3: str) -> None: ...
    @typing.overload
    def __init__(self, arg0: Pool, arg1: PrefixData) -> None: ...
    def add_extra_pkg_info(self, arg0: dict[str, ExtraPkgInfo]) -> None: ...
    def clear(self, arg0: bool) -> bool: ...
    def name(self) -> str: ...
    def priority(self) -> tuple[int, int]: ...
    def set_installed(self) -> None: ...
    def set_priority(self, arg0: int, arg1: int) -> None: ...
    def size(self) -> int: ...

class RoleBase:
    def all_keys(self) -> dict[str, RoleFullKeys]: ...
    @property
    def expired(self, arg1: TimeRef) -> bool: ...
    @property
    def expires(self) -> str: ...
    @property
    def file_ext(self) -> str: ...
    @property
    def spec_version(self) -> SpecBase: ...
    @property
    def type(self) -> str: ...
    @property
    def version(self) -> int: ...

class RoleBaseExtension:
    @property
    def timestamp(self) -> str: ...

class RoleFullKeys:
    keys: dict[str, Key]
    threshold: int
    @typing.overload
    def __init__(self) -> None: ...
    @typing.overload
    def __init__(self, keys: dict[str, Key], threshold: int) -> None: ...

class RootImpl(RoleBase, RoleBaseExtension):
    def __init__(self, json_str: str) -> None: ...
    def create_key_mgr(self, json_str: str) -> KeyMgr: ...
    def update(self, json_str: str) -> RootRole: ...

class RootRole:
    pass

class Solver:
    def __init__(self, arg0: Pool, arg1: list[tuple[int, int]]) -> None: ...
    def add_constraint(self, arg0: str) -> None: ...
    def add_global_job(self, arg0: int) -> None: ...
    def add_jobs(self, arg0: list[str], arg1: int) -> None: ...
    def add_pin(self, arg0: str) -> None: ...
    def all_problems_structured(self) -> list[SolverProblem]: ...
    def all_problems_to_str(self) -> str: ...
    def explain_problems(self) -> str: ...
    def is_solved(self) -> bool: ...
    def must_solve(self) -> None: ...
    def problems_to_str(self) -> str: ...
    def set_flags(self, arg0: list[tuple[int, int]]) -> None: ...
    def set_postsolve_flags(self, arg0: list[tuple[int, int]]) -> None: ...
    def solve(self) -> bool: ...
    def try_solve(self) -> bool: ...

class SolverProblem:
    dep: str | None
    dep_id: int
    description: str
    source: PackageInfo | None
    source_id: int
    target: PackageInfo | None
    target_id: int
    type: SolverRuleinfo
    def __str__(self) -> str: ...

class SolverRuleinfo:
    """
    Members:

      SOLVER_RULE_UNKNOWN

      SOLVER_RULE_PKG

      SOLVER_RULE_PKG_NOT_INSTALLABLE

      SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP

      SOLVER_RULE_PKG_REQUIRES

      SOLVER_RULE_PKG_SELF_CONFLICT

      SOLVER_RULE_PKG_CONFLICTS

      SOLVER_RULE_PKG_SAME_NAME

      SOLVER_RULE_PKG_OBSOLETES

      SOLVER_RULE_PKG_IMPLICIT_OBSOLETES

      SOLVER_RULE_PKG_INSTALLED_OBSOLETES

      SOLVER_RULE_PKG_RECOMMENDS

      SOLVER_RULE_PKG_CONSTRAINS

      SOLVER_RULE_UPDATE

      SOLVER_RULE_FEATURE

      SOLVER_RULE_JOB

      SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP

      SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM

      SOLVER_RULE_JOB_UNKNOWN_PACKAGE

      SOLVER_RULE_JOB_UNSUPPORTED

      SOLVER_RULE_DISTUPGRADE

      SOLVER_RULE_INFARCH

      SOLVER_RULE_CHOICE

      SOLVER_RULE_LEARNT

      SOLVER_RULE_BEST

      SOLVER_RULE_YUMOBS

      SOLVER_RULE_RECOMMENDS

      SOLVER_RULE_BLACK

      SOLVER_RULE_STRICT_REPO_PRIORITY
    """

    SOLVER_RULE_BEST: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_BEST: 2304>
    SOLVER_RULE_BLACK: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_BLACK: 3072>
    SOLVER_RULE_CHOICE: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_CHOICE: 1792>
    SOLVER_RULE_DISTUPGRADE: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_DISTUPGRADE: 1280>
    SOLVER_RULE_FEATURE: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_FEATURE: 768>
    SOLVER_RULE_INFARCH: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_INFARCH: 1536>
    SOLVER_RULE_JOB: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_JOB: 1024>
    SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP: 1025>
    SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM: 1026>
    SOLVER_RULE_JOB_UNKNOWN_PACKAGE: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_JOB_UNKNOWN_PACKAGE: 1027>
    SOLVER_RULE_JOB_UNSUPPORTED: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_JOB_UNSUPPORTED: 1028>
    SOLVER_RULE_LEARNT: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_LEARNT: 2048>
    SOLVER_RULE_PKG: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG: 256>
    SOLVER_RULE_PKG_CONFLICTS: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_CONFLICTS: 261>
    SOLVER_RULE_PKG_CONSTRAINS: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_CONSTRAINS: 267>
    SOLVER_RULE_PKG_IMPLICIT_OBSOLETES: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_IMPLICIT_OBSOLETES: 264>
    SOLVER_RULE_PKG_INSTALLED_OBSOLETES: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_INSTALLED_OBSOLETES: 265>
    SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP: 258>
    SOLVER_RULE_PKG_NOT_INSTALLABLE: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_NOT_INSTALLABLE: 257>
    SOLVER_RULE_PKG_OBSOLETES: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_OBSOLETES: 263>
    SOLVER_RULE_PKG_RECOMMENDS: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_RECOMMENDS: 266>
    SOLVER_RULE_PKG_REQUIRES: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_REQUIRES: 259>
    SOLVER_RULE_PKG_SAME_NAME: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_SAME_NAME: 262>
    SOLVER_RULE_PKG_SELF_CONFLICT: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_PKG_SELF_CONFLICT: 260>
    SOLVER_RULE_RECOMMENDS: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_RECOMMENDS: 2816>
    SOLVER_RULE_STRICT_REPO_PRIORITY: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_STRICT_REPO_PRIORITY: 3328>
    SOLVER_RULE_UNKNOWN: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_UNKNOWN: 0>
    SOLVER_RULE_UPDATE: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_UPDATE: 512>
    SOLVER_RULE_YUMOBS: typing.ClassVar[
        SolverRuleinfo
    ]  # value = <SolverRuleinfo.SOLVER_RULE_YUMOBS: 2560>
    __members__: typing.ClassVar[
        dict[str, SolverRuleinfo]
    ]  # value = {'SOLVER_RULE_UNKNOWN': <SolverRuleinfo.SOLVER_RULE_UNKNOWN: 0>, 'SOLVER_RULE_PKG': <SolverRuleinfo.SOLVER_RULE_PKG: 256>, 'SOLVER_RULE_PKG_NOT_INSTALLABLE': <SolverRuleinfo.SOLVER_RULE_PKG_NOT_INSTALLABLE: 257>, 'SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP': <SolverRuleinfo.SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP: 258>, 'SOLVER_RULE_PKG_REQUIRES': <SolverRuleinfo.SOLVER_RULE_PKG_REQUIRES: 259>, 'SOLVER_RULE_PKG_SELF_CONFLICT': <SolverRuleinfo.SOLVER_RULE_PKG_SELF_CONFLICT: 260>, 'SOLVER_RULE_PKG_CONFLICTS': <SolverRuleinfo.SOLVER_RULE_PKG_CONFLICTS: 261>, 'SOLVER_RULE_PKG_SAME_NAME': <SolverRuleinfo.SOLVER_RULE_PKG_SAME_NAME: 262>, 'SOLVER_RULE_PKG_OBSOLETES': <SolverRuleinfo.SOLVER_RULE_PKG_OBSOLETES: 263>, 'SOLVER_RULE_PKG_IMPLICIT_OBSOLETES': <SolverRuleinfo.SOLVER_RULE_PKG_IMPLICIT_OBSOLETES: 264>, 'SOLVER_RULE_PKG_INSTALLED_OBSOLETES': <SolverRuleinfo.SOLVER_RULE_PKG_INSTALLED_OBSOLETES: 265>, 'SOLVER_RULE_PKG_RECOMMENDS': <SolverRuleinfo.SOLVER_RULE_PKG_RECOMMENDS: 266>, 'SOLVER_RULE_PKG_CONSTRAINS': <SolverRuleinfo.SOLVER_RULE_PKG_CONSTRAINS: 267>, 'SOLVER_RULE_UPDATE': <SolverRuleinfo.SOLVER_RULE_UPDATE: 512>, 'SOLVER_RULE_FEATURE': <SolverRuleinfo.SOLVER_RULE_FEATURE: 768>, 'SOLVER_RULE_JOB': <SolverRuleinfo.SOLVER_RULE_JOB: 1024>, 'SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP': <SolverRuleinfo.SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP: 1025>, 'SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM': <SolverRuleinfo.SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM: 1026>, 'SOLVER_RULE_JOB_UNKNOWN_PACKAGE': <SolverRuleinfo.SOLVER_RULE_JOB_UNKNOWN_PACKAGE: 1027>, 'SOLVER_RULE_JOB_UNSUPPORTED': <SolverRuleinfo.SOLVER_RULE_JOB_UNSUPPORTED: 1028>, 'SOLVER_RULE_DISTUPGRADE': <SolverRuleinfo.SOLVER_RULE_DISTUPGRADE: 1280>, 'SOLVER_RULE_INFARCH': <SolverRuleinfo.SOLVER_RULE_INFARCH: 1536>, 'SOLVER_RULE_CHOICE': <SolverRuleinfo.SOLVER_RULE_CHOICE: 1792>, 'SOLVER_RULE_LEARNT': <SolverRuleinfo.SOLVER_RULE_LEARNT: 2048>, 'SOLVER_RULE_BEST': <SolverRuleinfo.SOLVER_RULE_BEST: 2304>, 'SOLVER_RULE_YUMOBS': <SolverRuleinfo.SOLVER_RULE_YUMOBS: 2560>, 'SOLVER_RULE_RECOMMENDS': <SolverRuleinfo.SOLVER_RULE_RECOMMENDS: 2816>, 'SOLVER_RULE_BLACK': <SolverRuleinfo.SOLVER_RULE_BLACK: 3072>, 'SOLVER_RULE_STRICT_REPO_PRIORITY': <SolverRuleinfo.SOLVER_RULE_STRICT_REPO_PRIORITY: 3328>}
    def __eq__(self, other: typing.Any) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: int) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: typing.Any) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: int) -> None: ...
    def __str__(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class SpecBase:
    pass

class SpecImpl(SpecBase):
    def __init__(self) -> None: ...

class SubdirData:
    def __init__(
        self, arg0: Channel, arg1: str, arg2: str, arg3: MultiPackageCache, arg4: str
    ) -> None: ...
    def cache_path(self) -> str: ...
    def create_repo(self, arg0: Pool) -> Repo: ...
    def download_and_check_targets(self, arg0: DownloadTargetList) -> bool: ...
    def finalize_checks(self) -> None: ...
    def loaded(self) -> bool: ...

class TimeRef:
    @typing.overload
    def __init__(self) -> None: ...
    @typing.overload
    def __init__(self, arg0: int) -> None: ...
    def set(self, arg0: int) -> None: ...
    def set_now(self) -> None: ...
    def timestamp(self) -> str: ...

class Transaction:
    @typing.overload
    def __init__(self, arg0: Solver, arg1: MultiPackageCache) -> None: ...
    @typing.overload
    def __init__(self, arg0: Pool, arg1: Solver, arg2: MultiPackageCache) -> None: ...
    def execute(self, arg0: PrefixData) -> bool: ...
    def fetch_extract_packages(self) -> bool: ...
    def find_python_version(self) -> tuple[str, str]: ...
    def log_json(self) -> None: ...
    def print(self) -> None: ...
    def prompt(self) -> bool: ...
    def to_conda(
        self,
    ) -> tuple[
        tuple[list[str], list[str]], list[tuple[str, str, str]], list[tuple[str, str]]
    ]: ...

class ostream_redirect:
    def __enter__(self) -> None: ...
    def __exit__(self, *args) -> None: ...
    def __init__(self, stdout: bool = ..., stderr: bool = ...) -> None: ...

def cache_fn_url(arg0: str) -> str: ...
def cancel_json_output() -> None: ...
def clean(arg0: int) -> None: ...
def create_cache_dir(arg0: Path) -> str: ...
def generate_ed25519_keypair() -> tuple[str, str]: ...
def get_channels(arg0: list[str]) -> list[Channel]: ...
def get_virtual_packages() -> list[PackageInfo]: ...
def init_console() -> None: ...
def sign(data: str, secret_key: str) -> str: ...
def simplify_conflicts(arg0: ProblemsGraph) -> ProblemsGraph: ...
def transmute(
    source_package: Path,
    destination_package: Path,
    compression_level: int,
    compression_threads: int = ...,
) -> bool: ...

MAMBA_CLEAN_ALL: int = 1
MAMBA_CLEAN_INDEX: int = 2
MAMBA_CLEAN_LOCKS: int = 16
MAMBA_CLEAN_PKGS: int = 4
MAMBA_CLEAN_TARBALLS: int = 8
MAMBA_DOWNLOAD_FAILFAST: int = 1
MAMBA_DOWNLOAD_SORT: int = 2
MAMBA_FORCE_REINSTALL: int = 4
MAMBA_NO_DEPS: int = 1
MAMBA_ONLY_DEPS: int = 2
SOLVER_ALLOWUNINSTALL: int = 2816
SOLVER_CLEANDEPS: int = 262144
SOLVER_DISFAVOR: int = 3328
SOLVER_DISTUPGRADE: int = 1792
SOLVER_DROP_ORPHANED: int = 2304
SOLVER_ERASE: int = 512
SOLVER_ESSENTIAL: int = 131072
SOLVER_FAVOR: int = 3072
SOLVER_FLAG_ADD_ALREADY_RECOMMENDED: int = 8
SOLVER_FLAG_ALLOW_ARCHCHANGE: int = 2
SOLVER_FLAG_ALLOW_DOWNGRADE: int = 1
SOLVER_FLAG_ALLOW_NAMECHANGE: int = 10
SOLVER_FLAG_ALLOW_UNINSTALL: int = 4
SOLVER_FLAG_ALLOW_VENDORCHANGE: int = 3
SOLVER_FLAG_BEST_OBEY_POLICY: int = 12
SOLVER_FLAG_BREAK_ORPHANS: int = 19
SOLVER_FLAG_DUP_ALLOW_ARCHCHANGE: int = 15
SOLVER_FLAG_DUP_ALLOW_DOWNGRADE: int = 14
SOLVER_FLAG_DUP_ALLOW_NAMECHANGE: int = 17
SOLVER_FLAG_DUP_ALLOW_VENDORCHANGE: int = 16
SOLVER_FLAG_FOCUS_BEST: int = 24
SOLVER_FLAG_FOCUS_INSTALLED: int = 20
SOLVER_FLAG_IGNORE_RECOMMENDED: int = 7
SOLVER_FLAG_INSTALL_ALSO_UPDATES: int = 26
SOLVER_FLAG_KEEP_EXPLICIT_OBSOLETES: int = 11
SOLVER_FLAG_KEEP_ORPHANS: int = 18
SOLVER_FLAG_NEED_UPDATEPROVIDE: int = 22
SOLVER_FLAG_NO_AUTOTARGET: int = 13
SOLVER_FLAG_NO_INFARCHCHECK: int = 9
SOLVER_FLAG_NO_UPDATEPROVIDE: int = 5
SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED: int = 27
SOLVER_FLAG_SPLITPROVIDES: int = 6
SOLVER_FLAG_STRICT_REPO_PRIORITY: int = 28
SOLVER_FLAG_STRONG_RECOMMENDS: int = 25
SOLVER_FLAG_URPM_REORDER: int = 23
SOLVER_FLAG_YUM_OBSOLETES: int = 21
SOLVER_FORCEBEST: int = 1048576
SOLVER_INSTALL: int = 256
SOLVER_JOBMASK: int = 65280
SOLVER_LOCK: int = 1536
SOLVER_MULTIVERSION: int = 1280
SOLVER_NOAUTOSET: int = 536870912
SOLVER_NOOP: int = 0
SOLVER_NOTBYUSER: int = 4194304
SOLVER_ORUPDATE: int = 524288
SOLVER_SELECTMASK: int = 255
SOLVER_SETARCH: int = 67108864
SOLVER_SETEV: int = 16777216
SOLVER_SETEVR: int = 33554432
SOLVER_SETMASK: int = 2130706432
SOLVER_SETNAME: int = 1073741824
SOLVER_SETREPO: int = 268435456
SOLVER_SETVENDOR: int = 134217728
SOLVER_SOLVABLE: int = 1
SOLVER_SOLVABLE_ALL: int = 6
SOLVER_SOLVABLE_NAME: int = 2
SOLVER_SOLVABLE_ONE_OF: int = 4
SOLVER_SOLVABLE_PROVIDES: int = 3
SOLVER_SOLVABLE_REPO: int = 5
SOLVER_TARGETED: int = 2097152
SOLVER_UPDATE: int = 768
SOLVER_USERINSTALLED: int = 2560
SOLVER_VERIFY: int = 2048
SOLVER_WEAK: int = 65536
SOLVER_WEAKENDEPS: int = 1024
