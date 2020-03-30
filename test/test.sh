#!/bin/sh
conda create -n xxx --override-channels --strict-channel-priority --dry-run -c ./test/channel_b -c ./test/channel_a a
mamba create -n xxx --override-channels --strict-channel-priority --dry-run -c ./test/channel_b -c ./test/channel_a a
conda create -n xxx --override-channels --strict-channel-priority --dry-run -c ./test/channel_b -c ./test/channel_a b
mamba create -n xxx --override-channels --strict-channel-priority --dry-run -c ./test/channel_b -c ./test/channel_a b
conda create -n xxx --override-channels --strict-channel-priority --dry-run -c ./test/channel_a -c ./test/channel_b b
mamba create -n xxx --override-channels --strict-channel-priority --dry-run -c ./test/channel_a -c ./test/channel_b b
