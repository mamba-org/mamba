# Import component submodules
# Try component module first, fall back to aggregated bindings
try:
    from libmambapy_common import utils, specs  # noqa: F401
except ImportError:
    from libmambapy.bindings import utils, specs  # noqa: F401

__all__ = ["utils", "specs"]
