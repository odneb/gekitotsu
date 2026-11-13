#!/bin/zsh
# Export native HUD-on frames; encoding can be done at 60 fps afterwards.
set -eu
script_dir="$(cd "$(dirname "$0")" && pwd)"
exec "$script_dir/preview_forest.sh" -RenderOffscreen -NoSound -NoSplash -UseFixedTimeStep -FPS=60 -gkscript -gkscriptfile="$script_dir/capture_cinematography.txt" "$@"
