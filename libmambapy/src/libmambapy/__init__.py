# Import all submodules so that one can use them directly with `import libmambapy`
import libmambapy.utils
import libmambapy.version
import libmambapy.specs
import libmambapy.solver

# Legacy which used to combine everything
from libmambapy.bindings.legacy import *  # noqa: F403

# Define top-level attributes
__version__ = libmambapy.version.__version__
