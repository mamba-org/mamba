# Import all submodules so that one can use them directly with `import libmambapy`
# Try to import from component modules first, fall back to aggregated bindings module
try:
    # Import from component modules if available (separate extension modules)
    import libmambapy_common  # noqa: F401
    import libmambapy_solver  # noqa: F401

    # Re-export component submodules
    from libmambapy_common import utils, specs  # noqa: F401
    from libmambapy_solver.solver import *  # noqa: F403

    # Import libsolv submodule
    try:
        import libmambapy_solver.solver.libsolv  # noqa: F401
    except ImportError:
        pass
except ImportError:
    # Fall back to aggregated bindings module for backward compatibility
    import libmambapy.bindings  # noqa: F401
    from libmambapy.bindings import utils, specs  # noqa: F401
    from libmambapy.bindings.solver import *  # noqa: F403

import libmambapy.version
import libmambapy.solver

# Legacy which used to combine everything
from libmambapy.bindings.legacy import *  # noqa: F403

# Define top-level attributes
__version__ = libmambapy.version.__version__
