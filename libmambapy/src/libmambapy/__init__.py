import libmambapy.bindings.specs
import libmambapy.version

# Legacy which used to combine everything
from libmambapy.bindings.legacy import *  # noqa: F403

# Define top-level attributes
__version__ = libmambapy.version.__version__

specs = libmambapy.bindings.specs
