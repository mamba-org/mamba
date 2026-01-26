# This file exists on its own rather than in `__init__.py` to make `import libmambapy.utils` work.
# Try component module first, fall back to aggregated bindings module
try:
    from libmambapy_common.utils import *  # noqa: F403
except ImportError:
    from libmambapy.bindings.utils import *  # noqa: F403
