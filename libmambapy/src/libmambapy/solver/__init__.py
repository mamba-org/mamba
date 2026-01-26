# Import solver submodules
# Try component module first, fall back to aggregated bindings
try:
    from libmambapy_solver.solver import *  # noqa: F403

    try:
        import libmambapy_solver.solver.libsolv  # noqa: F401
    except ImportError:
        pass
except ImportError:
    from libmambapy.bindings.solver import *  # noqa: F403

    try:
        import libmambapy.solver.libsolv  # noqa: F401
    except ImportError:
        pass
