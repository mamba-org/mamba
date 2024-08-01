import warnings

# This file exists on its own rather than in `__init__.py` to make `import libmambapy.solver.libsolv` work.
from libmambapy.bindings.solver.libsolv import *  # noqa: F403

from libmambapy.bindings.solver import (
    Priorities as _Priorities,
    RepodataParser as _RepodataParser,
    PipAsPythonDependency as _PipAsPythonDependency,
    PackageTypes as _PackageTypes,
    VerifyPackages as _VerifyPackages,
    RepodataOrigin as _RepodataOrigin,
)

_WARN_TEMPLATE_MSG = (
    "`libmambapy.bindings.solver.libsolv.{name}` has been moved "
    "to `libmambapy.bindings.solver.{name}` in 2.0. "
    "This import path will be removed in 2.2"
)


class Priorities(_Priorities):
    warnings.warn(
        _WARN_TEMPLATE_MSG.format(name="Priorities"),
        category=DeprecationWarning,
    )


class RepodataParser(_RepodataParser):
    warnings.warn(
        _WARN_TEMPLATE_MSG.format(name="RepodataParser"),
        category=DeprecationWarning,
    )


class PipAsPythonDependency(_PipAsPythonDependency):
    warnings.warn(
        _WARN_TEMPLATE_MSG.format(name="PipAsPythonDependency"),
        category=DeprecationWarning,
    )


class PackageTypes(_PackageTypes):
    warnings.warn(
        _WARN_TEMPLATE_MSG.format(name="PackageTypes"),
        category=DeprecationWarning,
    )


class VerifyPackages(_VerifyPackages):
    warnings.warn(
        _WARN_TEMPLATE_MSG.format(name="VerifyPackages"),
        category=DeprecationWarning,
    )


class RepodataOrigin(_RepodataOrigin):
    warnings.warn(
        _WARN_TEMPLATE_MSG.format(name="RepodataOrigin"),
        category=DeprecationWarning,
    )
