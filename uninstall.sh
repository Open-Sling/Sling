#!/bin/bash
echo "Testing for existing Sling installation..."
if command -v Sling &> /dev/null; then
    echo "Are you sure you want to uninstall Sling? (y/N)"
    read -r response
    if [[ "$response" != "y" && "$response" != "Y" ]]; then
        echo "Uninstallation cancelled."
        exit 0
    else
        echo "Proceeding with uninstallation..."
        set -euo pipefail
        echo "Uninstalling Sling..."
        sudo rm /usr/local/bin/Sling
        echo "Uninstallation complete."
        exit 0
    fi
else
    echo "No existing Sling installation found. Nothing to uninstall."
    exit 0
fi