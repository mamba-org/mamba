import libmambapy.core.version
from libmambapy.core.bindings import *  # Legacy which used to combine everything

# Shim submodules as core submodules
version = libmambapy.core.version

# Define top-level attributes
__version__ = version.__version__
