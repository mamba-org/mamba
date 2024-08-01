import warnings

# This file exists on its own rather than in `__init__.py` to make `import libmambapy.solver.libsolv` work.
from libmambapy.bindings.solver.libsolv import *  # noqa: F403

from libmambapy.bindings.solver import UnSolvable as _UnSolvable


_WARN_TEMPLATE_MSG = (
    "`libmambapy.bindings.solver.libsolv.{name}` has been moved "
    "to `libmambapy.bindings.solver.{name}` in 2.0. "
    "This import path will be removed in 2.2"
)


class UnSolvable(_UnSolvable):
    warnings.warn(
        _WARN_TEMPLATE_MSG.format(name="UnSolvable"),
        category=DeprecationWarning,
    )
