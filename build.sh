#!/bin/bash
origDir=$(pwd)
pebble kill
cd ~
rm -rf ~/MATLAB_Time_New
cp -r /mnt/c/MATLAB/Claude/MATLAB_Time_New ~/MATLAB_Time_New
cd ~/MATLAB_Time_New
rm -rf build
find /var/tmp/pebble-sdk -name "*.xsa" -delete 2>/dev/null
pebble wipe
pebble clean
pebble build
pebble install --emulator gabbro
cd "$origDir"
