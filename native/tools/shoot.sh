#!/bin/bash
# shoot.sh <script file in Saved> : runs the game on that script and waits for it
cd "$HOME/mnt/Epic Games/GekitotsuNative"
cp "Saved/$1" Saved/script.txt; mkdir -p Saved/shots_old; mv Saved/shots/*.png Saved/shots_old/ 2>/dev/null
echo "-gkscript -windowed -ResX=${2:-640} -ResY=${3:-360}" > Saved/run_args.txt
sleep 8; tools/trig.sh run 15
