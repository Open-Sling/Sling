#!/bin/bash
set -euo pipefail
echo "Testing for existing Sling installation..."

# Accept --force to reinstall even if Sling is present
FORCE=0
if [[ "${1-}" == "--force" ]]; then
    FORCE=1
fi

if command -v Sling &> /dev/null && [[ $FORCE -eq 0 ]]; then
        echo "Sling is already installed. Use 'Sling -v' to check the version or run 'bash build.sh --force' to reinstall."
        exit 0
fi

echo "Proceeding with build..."
echo "Building Sling runtime and native module..."

# Compile runtime (emit SDK globals in this TU)
gcc -DSDK_IMPLEMENTATION -Iruntime -Wall -Wextra -c runtime/sling.c -o sling.o

# Compile native module without SDK_IMPLEMENTATION
gcc -Iruntime -Wall -Wextra -c runtime/module_stubs.c -o module_stubs.o

# Link executable (install name: Sling)
gcc sling.o module_stubs.o -o Sling

# Clean up object files
rm -f sling.o module_stubs.o

echo "Installing Sling to /usr/local/bin (requires sudo)..."
sudo cp Sling /usr/local/bin/Sling
echo "Build and install complete. Run 'Sling -v' to verify installation."

# If the hello test exists, run it with the installed Sling to verify behavior
if [ -f examples/hello_test.sling ]; then
    echo "Running examples/hello_test.sling with installed Sling to verify native module..."
    /usr/local/bin/Sling examples/hello_test.sling || true
fi