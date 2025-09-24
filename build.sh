#!/bin/bash
set -euo pipefail

# Build script: always build the local `Sling` binary first, then
# compare it with /usr/local/bin/Sling (sha256). Install only if
# different or when --force is passed.

FORCE=0
if [[ "${1-}" == "--force" ]]; then
    FORCE=1
fi

echo "Proceeding with build..."
echo "Building Sling runtime and native module..."

# Compile runtime (emit SDK globals in this TU)
gcc -DSDK_IMPLEMENTATION -Iruntime -Wall -Wextra -c runtime/sling.c -o sling.o

# Compile native module without SDK_IMPLEMENTATION
gcc -Iruntime -Wall -Wextra -c runtime/module_stubs.c -o module_stubs.o

# Link executable (local binary: Sling)
gcc sling.o module_stubs.o -o Sling

# Compute checksums (use shasum which exists on macOS; fallback when not present)
NEWSUM=""
OLDSUM=""
if command -v shasum >/dev/null 2>&1; then
    NEWSUM=$(shasum -a 256 Sling | awk '{print $1}')
    if [ -f /usr/local/bin/Sling ]; then
        OLDSUM=$(shasum -a 256 /usr/local/bin/Sling | awk '{print $1}') || true
    fi
elif command -v sha256sum >/dev/null 2>&1; then
    NEWSUM=$(sha256sum Sling | awk '{print $1}')
    if [ -f /usr/local/bin/Sling ]; then
        OLDSUM=$(sha256sum /usr/local/bin/Sling | awk '{print $1}') || true
    fi
fi

if [[ $FORCE -eq 0 && -n "$OLDSUM" && "$NEWSUM" == "$OLDSUM" ]]; then
    echo "/usr/local/bin/Sling is already the current build (checksums match). Skipping install."
else
    echo "Installing Sling to /usr/local/bin (requires sudo)..."
    sudo cp Sling /usr/local/bin/Sling
    echo "Build and install complete."
fi

# Clean up object files
rm -f sling.o module_stubs.o

# Optional verification: run hello test if present
if [ -f examples/hello_test.sling ]; then
    echo "Running examples/hello_test.sling with installed Sling to verify native module..."
    /usr/local/bin/Sling examples/hello_test.sling || true
fi