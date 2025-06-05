import libmambapy.bindings.specs
import libmambapy.bindings.utils
from . import libsolv as libsolv
from _typeshed import Incomplete
from typing import ClassVar, Iterable, Iterator, overload

class CompressedProblemsGraph:
    class ConstraintListNode:
        def __init__(self) -> None: ...
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
        def __bool__(self) -> bool: ...
        def __iter__(self) -> Iterator[ProblemsGraph.ConstraintNode]: ...
        def __len__(self) -> int: ...

    class DependencyList:
        def __init__(self) -> None: ...
        def add(self, arg0: libmambapy.bindings.specs.MatchSpec) -> None: ...
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
        def __bool__(self) -> bool: ...
        def __iter__(self) -> Iterator[libmambapy.bindings.specs.MatchSpec]: ...
        def __len__(self) -> int: ...

    class PackageListNode:
        def __init__(self) -> None: ...
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
        def __bool__(self) -> bool: ...
        def __iter__(self) -> Iterator[ProblemsGraph.PackageNode]: ...
        def __len__(self) -> int: ...

    class UnresolvedDependencyListNode:
        def __init__(self) -> None: ...
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
        def __bool__(self) -> bool: ...
        def __iter__(self) -> Iterator[ProblemsGraph.UnresolvedDependencyNode]: ...
        def __len__(self) -> int: ...

    ConflictMap: ClassVar[type] = ...  # read-only
    RootNode: ClassVar[type] = ...  # read-only
    def __init__(self, *args, **kwargs) -> None: ...
    def conflicts(self) -> ProblemsGraph.ConflictMap: ...
    @overload
    @staticmethod
    def from_problems_graph(
        problems_graph: ProblemsGraph, merge_criteria
    ) -> CompressedProblemsGraph: ...
    @overload
    @staticmethod
    def from_problems_graph(problems_graph: ProblemsGraph) -> CompressedProblemsGraph: ...
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
    def tree_message(self, format: ProblemsMessageFormat = ...) -> str: ...

class ProblemsGraph:
    class ConflictMap:
        def __init__(self) -> None: ...
        def add(self, arg0: int, arg1: int) -> bool: ...
        def clear(self) -> None: ...
        def conflicts(self, arg0: int) -> set[int]: ...
        def has_conflict(self, arg0: int) -> bool: ...
        def in_conflict(self, arg0: int, arg1: int) -> bool: ...
        def __bool__(self) -> bool: ...
        def __contains__(self, arg0: int) -> bool: ...
        def __copy__(self) -> ProblemsGraph.ConflictMap: ...
        def __deepcopy__(self, memo: dict) -> ProblemsGraph.ConflictMap: ...
        def __eq__(self, arg0: ProblemsGraph.ConflictMap) -> bool: ...
        def __iter__(self) -> Iterator[tuple[int, set[int]]]: ...
        def __len__(self) -> int: ...
        def __ne__(self, arg0: ProblemsGraph.ConflictMap) -> bool: ...

    class ConstraintNode(libmambapy.bindings.specs.MatchSpec):
        def __init__(self, arg0: libmambapy.bindings.specs.MatchSpec) -> None: ...

    class PackageNode(libmambapy.bindings.specs.PackageInfo):
        def __init__(self, arg0: libmambapy.bindings.specs.PackageInfo) -> None: ...

    class RootNode:
        def __init__(self) -> None: ...

    class UnresolvedDependencyNode(libmambapy.bindings.specs.MatchSpec):
        def __init__(self, arg0: libmambapy.bindings.specs.MatchSpec) -> None: ...

    def __init__(self, *args, **kwargs) -> None: ...
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
        dict[tuple[int, int], libmambapy.bindings.specs.MatchSpec],
    ]: ...
    def root_node(self) -> int: ...
    @staticmethod
    def simplify_conflicts(arg0: ProblemsGraph) -> ProblemsGraph: ...

class ProblemsMessageFormat:
    available: libmambapy.bindings.utils.TextStyle
    indents: Incomplete
    unavailable: libmambapy.bindings.utils.TextStyle
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(
        self,
        unavailable: libmambapy.bindings.utils.TextStyle,
        available: libmambapy.bindings.utils.TextStyle,
        indents,
    ) -> None: ...
    def __copy__(self) -> ProblemsMessageFormat: ...
    def __deepcopy__(self, memo: dict) -> ProblemsMessageFormat: ...

class Request:
    class Flags:
        allow_downgrade: bool
        allow_uninstall: bool
        force_reinstall: bool
        keep_dependencies: bool
        keep_user_specs: bool
        order_request: bool
        strict_repo_priority: bool
        def __init__(
            self,
            keep_dependencies: bool = ...,
            keep_user_specs: bool = ...,
            force_reinstall: bool = ...,
            allow_downgrade: bool = ...,
            allow_uninstall: bool = ...,
            strict_repo_priority: bool = ...,
            order_request: bool = ...,
        ) -> None: ...
        def __copy__(self) -> Request.Flags: ...
        def __deepcopy__(self, memo: dict) -> Request.Flags: ...

    class Freeze:
        spec: libmambapy.bindings.specs.MatchSpec
        def __init__(self, spec: libmambapy.bindings.specs.MatchSpec) -> None: ...
        def __copy__(self) -> Request.Freeze: ...
        def __deepcopy__(self, memo: dict) -> Request.Freeze: ...

    class Install:
        spec: libmambapy.bindings.specs.MatchSpec
        def __init__(self, spec: libmambapy.bindings.specs.MatchSpec) -> None: ...
        def __copy__(self) -> Request.Install: ...
        def __deepcopy__(self, memo: dict) -> Request.Install: ...

    class JobList:
        @overload
        def __init__(self) -> None: ...
        @overload
        def __init__(self, arg0: Request.JobList) -> None: ...
        @overload
        def __init__(self, arg0: Iterable) -> None: ...
        def append(
            self,
            x: Request.Install
            | Request.Remove
            | Request.Update
            | Request.UpdateAll
            | Request.Keep
            | Request.Freeze
            | Request.Pin,
        ) -> None: ...
        def clear(self) -> None: ...
        @overload
        def extend(self, L: Request.JobList) -> None: ...
        @overload
        def extend(self, L: Iterable) -> None: ...
        def insert(
            self,
            i: int,
            x: Request.Install
            | Request.Remove
            | Request.Update
            | Request.UpdateAll
            | Request.Keep
            | Request.Freeze
            | Request.Pin,
        ) -> None: ...
        @overload
        def pop(
            self,
        ) -> (
            Request.Install
            | Request.Remove
            | Request.Update
            | Request.UpdateAll
            | Request.Keep
            | Request.Freeze
            | Request.Pin
        ): ...
        @overload
        def pop(
            self, i: int
        ) -> (
            Request.Install
            | Request.Remove
            | Request.Update
            | Request.UpdateAll
            | Request.Keep
            | Request.Freeze
            | Request.Pin
        ): ...
        def __bool__(self) -> bool: ...
        @overload
        def __delitem__(self, arg0: int) -> None: ...
        @overload
        def __delitem__(self, arg0: slice) -> None: ...
        @overload
        def __getitem__(self, s: slice) -> Request.JobList: ...
        @overload
        def __getitem__(
            self, arg0: int
        ) -> (
            Request.Install
            | Request.Remove
            | Request.Update
            | Request.UpdateAll
            | Request.Keep
            | Request.Freeze
            | Request.Pin
        ): ...
        def __iter__(
            self,
        ) -> Iterator[
            Request.Install
            | Request.Remove
            | Request.Update
            | Request.UpdateAll
            | Request.Keep
            | Request.Freeze
            | Request.Pin
        ]: ...
        def __len__(self) -> int: ...
        @overload
        def __setitem__(
            self,
            arg0: int,
            arg1: Request.Install
            | Request.Remove
            | Request.Update
            | Request.UpdateAll
            | Request.Keep
            | Request.Freeze
            | Request.Pin,
        ) -> None: ...
        @overload
        def __setitem__(self, arg0: slice, arg1: Request.JobList) -> None: ...

    class Keep:
        spec: libmambapy.bindings.specs.MatchSpec
        def __init__(self, spec: libmambapy.bindings.specs.MatchSpec) -> None: ...
        def __copy__(self) -> Request.Keep: ...
        def __deepcopy__(self, memo: dict) -> Request.Keep: ...

    class Pin:
        spec: libmambapy.bindings.specs.MatchSpec
        def __init__(self, spec: libmambapy.bindings.specs.MatchSpec) -> None: ...
        def __copy__(self) -> Request.Pin: ...
        def __deepcopy__(self, memo: dict) -> Request.Pin: ...

    class Remove:
        clean_dependencies: bool
        spec: libmambapy.bindings.specs.MatchSpec
        def __init__(
            self, spec: libmambapy.bindings.specs.MatchSpec, clean_dependencies: bool = ...
        ) -> None: ...
        def __copy__(self) -> Request.Remove: ...
        def __deepcopy__(self, memo: dict) -> Request.Remove: ...

    class Update:
        clean_dependencies: bool
        spec: libmambapy.bindings.specs.MatchSpec
        def __init__(
            self, spec: libmambapy.bindings.specs.MatchSpec, clean_dependencies: bool = ...
        ) -> None: ...
        def __copy__(self) -> Request.Update: ...
        def __deepcopy__(self, memo: dict) -> Request.Update: ...

    class UpdateAll:
        clean_dependencies: bool
        def __init__(self, clean_dependencies: bool = ...) -> None: ...
        def __copy__(self) -> Request.UpdateAll: ...
        def __deepcopy__(self, memo: dict) -> Request.UpdateAll: ...

    flags: Request.Flags
    jobs: Request.JobList
    @overload
    def __init__(self, jobs: Request.JobList, flags: Request.Flags = ...) -> None: ...
    @overload
    def __init__(self, jobs: Iterable, flags: Request.Flags = ...) -> None: ...
    def __copy__(self) -> Request: ...
    def __deepcopy__(self, memo: dict) -> Request: ...

class Solution:
    class ActionList:
        @overload
        def __init__(self) -> None: ...
        @overload
        def __init__(self, arg0: Solution.ActionList) -> None: ...
        @overload
        def __init__(self, arg0: Iterable) -> None: ...
        def append(
            self,
            x: Solution.Omit
            | Solution.Upgrade
            | Solution.Downgrade
            | Solution.Change
            | Solution.Reinstall
            | Solution.Remove
            | Solution.Install,
        ) -> None: ...
        def clear(self) -> None: ...
        @overload
        def extend(self, L: Solution.ActionList) -> None: ...
        @overload
        def extend(self, L: Iterable) -> None: ...
        def insert(
            self,
            i: int,
            x: Solution.Omit
            | Solution.Upgrade
            | Solution.Downgrade
            | Solution.Change
            | Solution.Reinstall
            | Solution.Remove
            | Solution.Install,
        ) -> None: ...
        @overload
        def pop(
            self,
        ) -> (
            Solution.Omit
            | Solution.Upgrade
            | Solution.Downgrade
            | Solution.Change
            | Solution.Reinstall
            | Solution.Remove
            | Solution.Install
        ): ...
        @overload
        def pop(
            self, i: int
        ) -> (
            Solution.Omit
            | Solution.Upgrade
            | Solution.Downgrade
            | Solution.Change
            | Solution.Reinstall
            | Solution.Remove
            | Solution.Install
        ): ...
        def __bool__(self) -> bool: ...
        @overload
        def __delitem__(self, arg0: int) -> None: ...
        @overload
        def __delitem__(self, arg0: slice) -> None: ...
        @overload
        def __getitem__(self, s: slice) -> Solution.ActionList: ...
        @overload
        def __getitem__(
            self, arg0: int
        ) -> (
            Solution.Omit
            | Solution.Upgrade
            | Solution.Downgrade
            | Solution.Change
            | Solution.Reinstall
            | Solution.Remove
            | Solution.Install
        ): ...
        def __iter__(
            self,
        ) -> Iterator[
            Solution.Omit
            | Solution.Upgrade
            | Solution.Downgrade
            | Solution.Change
            | Solution.Reinstall
            | Solution.Remove
            | Solution.Install
        ]: ...
        def __len__(self) -> int: ...
        @overload
        def __setitem__(
            self,
            arg0: int,
            arg1: Solution.Omit
            | Solution.Upgrade
            | Solution.Downgrade
            | Solution.Change
            | Solution.Reinstall
            | Solution.Remove
            | Solution.Install,
        ) -> None: ...
        @overload
        def __setitem__(self, arg0: slice, arg1: Solution.ActionList) -> None: ...

    class Change:
        install: libmambapy.bindings.specs.PackageInfo
        remove: libmambapy.bindings.specs.PackageInfo
        def __init__(
            self,
            remove: libmambapy.bindings.specs.PackageInfo,
            install: libmambapy.bindings.specs.PackageInfo,
        ) -> None: ...
        def __copy__(self) -> Solution.Change: ...
        def __deepcopy__(self, memo: dict) -> Solution.Change: ...

    class Downgrade:
        install: libmambapy.bindings.specs.PackageInfo
        remove: libmambapy.bindings.specs.PackageInfo
        def __init__(
            self,
            remove: libmambapy.bindings.specs.PackageInfo,
            install: libmambapy.bindings.specs.PackageInfo,
        ) -> None: ...
        def __copy__(self) -> Solution.Downgrade: ...
        def __deepcopy__(self, memo: dict) -> Solution.Downgrade: ...

    class Install:
        install: libmambapy.bindings.specs.PackageInfo
        def __init__(self, install: libmambapy.bindings.specs.PackageInfo) -> None: ...
        def __copy__(self) -> Solution.Install: ...
        def __deepcopy__(self, memo: dict) -> Solution.Install: ...

    class Omit:
        what: libmambapy.bindings.specs.PackageInfo
        def __init__(self, what: libmambapy.bindings.specs.PackageInfo) -> None: ...
        def __copy__(self) -> Solution.Omit: ...
        def __deepcopy__(self, memo: dict) -> Solution.Omit: ...

    class Reinstall:
        what: libmambapy.bindings.specs.PackageInfo
        def __init__(self, what: libmambapy.bindings.specs.PackageInfo) -> None: ...
        def __copy__(self) -> Solution.Reinstall: ...
        def __deepcopy__(self, memo: dict) -> Solution.Reinstall: ...

    class Remove:
        remove: libmambapy.bindings.specs.PackageInfo
        def __init__(self, remove: libmambapy.bindings.specs.PackageInfo) -> None: ...
        def __copy__(self) -> Solution.Remove: ...
        def __deepcopy__(self, memo: dict) -> Solution.Remove: ...

    class Upgrade:
        install: libmambapy.bindings.specs.PackageInfo
        remove: libmambapy.bindings.specs.PackageInfo
        def __init__(
            self,
            remove: libmambapy.bindings.specs.PackageInfo,
            install: libmambapy.bindings.specs.PackageInfo,
        ) -> None: ...
        def __copy__(self) -> Solution.Upgrade: ...
        def __deepcopy__(self, memo: dict) -> Solution.Upgrade: ...

    actions: Solution.ActionList
    @overload
    def __init__(self, arg0: Solution.ActionList) -> None: ...
    @overload
    def __init__(self, arg0: Iterable) -> None: ...
    def to_install(self) -> list[libmambapy.bindings.specs.PackageInfo]: ...
    def to_omit(self) -> list[libmambapy.bindings.specs.PackageInfo]: ...
    def to_remove(self) -> list[libmambapy.bindings.specs.PackageInfo]: ...
    def __copy__(self) -> Solution: ...
    def __deepcopy__(self, memo: dict) -> Solution: ...
