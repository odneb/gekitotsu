#!/bin/zsh
# Native combat checks, independent of the Unreal Editor.
set -eu
native_dir="$(cd "$(dirname "$0")/.." && pwd)"
check_dir="$(mktemp -d "${TMPDIR:-/tmp}/gk-film-check.XXXXXX")"
trap 'rm -rf "$check_dir"' EXIT
for name in controls beam moves; do
    clang++ -O2 -std=c++17 -I"$native_dir/Source/GekitotsuNative/Core" \
        "$native_dir/tools/check_$name.cpp" -o "$check_dir/check_$name"
    "$check_dir/check_$name"
done
node "$native_dir/tools/gen_moves.js" "$native_dir/../src/sim.js" > "$check_dir/gk_moves.inl"
cmp "$check_dir/gk_moves.inl" "$native_dir/Source/GekitotsuNative/Core/gk_moves.inl"
print 'PASS: generated move table matches its source'
