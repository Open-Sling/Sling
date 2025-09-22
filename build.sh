#!/bin/bash
set -euo pipefail
echo "Testing for exesting Sling installation..."
if command -v Sling &> /dev/null; then
    echo "Sling is already installed. Use 'Sling -v' to check the version."
    exit 0
else
    echo "No existing Sling installation found. Proceeding with build..."
    echo "Building Sling runtime and native module..."

    # Compile runtime (emit SDK globals in this TU)
    gcc -DSDK_IMPLEMENTATION -Iruntime -Wall -Wextra -c runtime/sling.c -o sling.o

    # Compile native module without SDK_IMPLEMENTATION
    gcc -Iruntime -Wall -Wextra -c runtime/module_stubs.c -o module_stubs.o

    # Link executable
    gcc sling.o module_stubs.o -o Sling
    # Clean up object files
    rm *.o
    sudo cp Sling /usr/local/bin/Sling
    echo "Build complete: Run Sling -v to verify installation."
fi