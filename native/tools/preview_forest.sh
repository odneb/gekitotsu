#!/bin/zsh
# Launch the native match with optional Unreal arguments such as -gkpalette=1.
set -eu
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
engine_dir="${GK_UE_ENGINE:-/Users/Shared/Epic Games/UE_5.8/Engine}"
project="$project_dir/GekitotsuNative.uproject"
forest='/Game/DreamscapeSeries/DreamscapeDreadmarsh/Maps/Dreadmarsh_DemoMap_Resize?game=/Script/GekitotsuNative.GkGameMode'

if [[ ! -f "$project_dir/Content/DreamscapeSeries/DreamscapeDreadmarsh/Maps/Dreadmarsh_DemoMap_Resize.umap" ]]; then
    print -u2 'Copy the existing native project Content into this checkout before launching.'
    exit 1
fi

exec "$engine_dir/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$project" "$forest" -game -gkfight -windowed -ResX=1280 -ResY=720 "$@"
