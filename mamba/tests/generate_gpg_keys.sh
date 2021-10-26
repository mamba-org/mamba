#!/bin/bash

set -euo pipefail -x

cat << EOF | gpg --batch --generate-key
%no-protection
Key-Type: eddsa
Key-Curve: Ed25519
Key-Usage: sign
Name-Real: MAMBA1
Name-Email: mail at example.com
Creation-Date: 20170801T180000
Expire-Date: 0
Subkey-Type: ecdh
Subkey-Curve: Curve25519
Subkey-Usage: encrypt
EOF

cat << EOF | gpg --batch --generate-key
%no-protection
Key-Type: eddsa
Key-Curve: Ed25519
Key-Usage: sign
Name-Real: MAMBA2
Name-Email: mail at example.com
Creation-Date: 20170801T180000
Expire-Date: 0
Subkey-Type: ecdh
Subkey-Curve: Curve25519
Subkey-Usage: encrypt
EOF
