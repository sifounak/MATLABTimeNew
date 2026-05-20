#!/bin/bash
origDir=$(pwd)
cd ~
rm -rf ~/MATLAB_Time_New
cp -r /mnt/c/MATLAB/Claude/MATLAB_Time_New ~/MATLAB_Time_New
cd ~/MATLAB_Time_New
pebble wipe && pebble build && pebble install --emulator gabbro
cd "$origDir"
