# This file exists on its own rather than in `__init__.py` to make `import libmambapy.solver.libsolv` work.
# Try component module first, fall back to aggregated bindings module
try:
    from libmambapy_solver.solver.libsolv import *  # noqa: F403
except ImportError:
    from libmambapy.bindings.solver.libsolv import *  # noqa: F403
