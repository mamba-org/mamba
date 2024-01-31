#!/usr/bin/env python
import signal
import os
from time import sleep

# Define the sleep time
sleep_time = 0.005

# Get all valid signals and remove the unhandled ones.
# SIGKILL and SIGSTOP cannot be caught, while SIGCHLD
# is not forwarded by umamba because it makes no sense to do so
all_signals = signal.valid_signals() - {signal.SIGKILL, signal.SIGCHLD, signal.SIGSTOP}

# Initialize a dictionary to keep track of received signals.
# Keep it global to have it accessible from the signal handler.
received_signals = {int(s): False for s in all_signals}

# Define the signal handler
def signal_handler(sig, _):
    received_signals[sig] = True

def set_signal_handlers():
    """Set signal handlers for all signals."""
    for s in all_signals:
        signal.signal(s, signal_handler)

def send_signals():
    """Send all signals to the parent process."""
    ppid = os.getppid()
    for s in all_signals:
        os.kill(ppid, s)
        # wait so that the handlers are executed (and in order)
        sleep(sleep_time)

def main():
    set_signal_handlers()
    sleep(sleep_time)

    # Send signals and give time for the handlers to be called
    send_signals()

    ok = all(received_signals.values())
    if not ok:
        # Print the signals that were not handled
        unhandled_signals = [k for k, v in received_signals.items() if not v]
        print(f"No signal handled for signals {unhandled_signals}")
        exit(1)

    print("Signal forwarding ok")

if __name__ == "__main__":
    main()
