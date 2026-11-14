#!/bin/zsh
# Regenerate local derivatives of owned packs; never edits vendor source assets.
set -eu
native_dir="$(cd "$(dirname "$0")/.." && pwd)"
engine_root="${GK_ENGINE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$engine_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
[[ -x "$editor" ]] || { print -u2 "Set GK_ENGINE_ROOT to your Unreal Engine installation."; exit 1; }
mkdir -p "$native_dir/Saved/RenderSetup"
for script in refine_forest_surfaces refine_swamp_ground refine_daylight_materials refine_swamp_vapor; do
    "$editor" "$native_dir/GekitotsuNative.uproject" -run=pythonscript \
        "-script=$native_dir/tools/$script.py" -unattended -nullrhi -nosound \
        "-abslog=$native_dir/Saved/RenderSetup/$script.log" \
        > "$native_dir/Saved/RenderSetup/$script-console.log" 2>&1
    print "Prepared $script"
done
