import time
from libmambapy import Context, ContextOptions

# mamba_options = ContextOptions()
# mamba_options.enable_logging_and_signal_handling = False
# mamba_context = Context(mamba_options) # Also initializes the still-singletons from mamba, and bind their lifetime to this object
mamba_context = Context(ContextOptions(enable_logging_and_signal_handling = False))

print("Sleep for 10s... Try to Ctrl+C")
time.sleep(10)

