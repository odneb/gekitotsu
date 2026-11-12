#!/bin/bash
# trig.sh build|run|wait  : start a build (or a launch) through the editor's watcher and wait for it; "wait" only waits.
cd "$HOME/mnt/Epic Games/GekitotsuNative"
if [ "$1" = build ]; then cp Saved/package.done Saved/package.prev 2>/dev/null; cp tools/build.py Content/Python/test.py; echo "# $(date +%s)" >> Content/Python/test.py; sleep 12; fi
if [ "$1" = run ]; then mv Saved/shots/done.txt Saved/shots/done.old 2>/dev/null; cp tools/run_args.py Content/Python/test.py; echo "# $(date +%s)" >> Content/Python/test.py; for i in $(seq 1 ${2:-15}); do [ -f Saved/shots/done.txt ] && break; sleep 10; done; ls Saved/shots | wc -l; [ -f Saved/shots/done.txt ] && echo SCRIPT DONE; exit 0; fi
for i in $(seq 1 15); do cmp -s Saved/package.done Saved/package.prev || break; grep -q "error:" Saved/package.log 2>/dev/null && [ $i -gt 3 ] && break; sleep 10; done
if cmp -s Saved/package.done Saved/package.prev; then echo "STILL BUILDING: $(tail -1 Saved/package.log | cut -c1-120)"; else echo "DONE: $(cat Saved/package.done)"; fi
grep "error" Saved/package.log | grep -v "0 error\|Warning/Error" | head -12 | cut -c1-330
