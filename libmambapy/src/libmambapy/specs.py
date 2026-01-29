# This file exists on its own rather than in `__init__.py` to make `import libmambapy.specs` work.
# Try component module first, fall back to aggregated bindings module
try:
    from libmambapy_common.specs import *  # noqa: F403
except ImportError:
    from libmambapy.bindings.specs import *  # noqa: F403
