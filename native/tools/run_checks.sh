#!/bin/sh
# Plays the browser rules (Node) and the C++ rules side by side over many matches and reports any frame that differs.
# usage: tools/run_checks.sh <browser game dir> [frames per run] [seeds]
G=${1:-/home/claude/game}; N=${2:-100000}; SEEDS=${3:-"1 2 3"}; fail=0; total=0
for m in random mash cpu cpuhuman training force; do for sd in $SEEDS; do case $m in cpu) A="-1 1";; cpuhuman) A="-1 2";; *) A="";; esac
  node tools/oracle.js "$G" $m $N $sd build/o.txt >/dev/null && r=$(./build/gkcheck build/o.txt $A); echo "$m seed $sd: $r"; total=$((total+N)); case "$r" in IDENTICAL*) ;; *) fail=1;; esac
done; done
[ $fail = 0 ] && echo "ALL IDENTICAL: $total frames" || echo "SOME RUNS DIFFER"
