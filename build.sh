#!/bin/bash

# Build and install Pebble watch face
# Must be run from WSL

# Check inputs
targetPlatform="gabbro"
if [ $# -gt 0 ]; then
    targetPlatform="$1"
fi

origDir=$(pwd)
scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
workDir="$HOME/MATLAB_Time_New"
pebble kill
rm -rf "$workDir"
cp -r "$scriptDir" "$workDir"
cd "$workDir"
rm -rf build node_modules
pebble wipe
pebble clean
pebble build
pebble install --emulator "${targetPlatform}" --logs
cd "$origDir"
