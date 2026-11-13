# GEKITOTSU, native Unreal Engine 5 project

A fresh project. Nothing from the earlier Unreal attempt (`ue5/`) is used.

- `Source/GekitotsuNative/Core/` is the combat simulation and CPU opponent: plain C++, fixed 60 Hz, integer-only,
  a function-for-function translation of the browser game's `sim.js` and its CPU. `gk_moves.inl` is generated from
  the browser's move table by `tools/gen_moves.js`.
- `GkMatch.*` steps the simulation in real time and shows it; `GkHud.cpp` draws the working HUD; `GkPad.*` reads
  fight sticks straight from USB on the Mac.
- `tools/run_checks.sh` plays the browser rules (under Node) and the C++ side by side and reports any frame that
  differs. Test tool only: the game runs no JavaScript.

Fighters (Epic's mannequins, copied from the engine's template content into `Content/Characters`) and the stage are
placeholders for the first milestone.

## Dreadmarsh forest preview on Mac

Use Unreal Engine 5.8.4 and Xcode. The repository contains the native source and the
forest post-process material. The existing project's local Content supplies the
Dreamscape map, fighters, and effects. Copy those assets into a checkout without
overwriting tracked files.

Build the editor target and launch a standalone fight:

```sh
engine='/Users/Shared/Epic Games/UE_5.8/Engine'
"$engine/Build/BatchFiles/Mac/Build.sh" GekitotsuNativeEditor Mac Development "-Project=$PWD/GekitotsuNative.uproject"
tools/preview_forest.sh
```

Run those commands from the native project directory. The launcher selects GEKITOTSU's
game mode on the vendor map. From the normal game menu, choose **Dreadmarsh Swamp**.
The standard match starts in daylight and can blend into twilight and a storm as
rounds progress and health falls. To audition one palette, append an option to
the preview command:

| Option | Palette |
| --- | --- |
| `-gkpalette=0` | Daylight |
| `-gkpalette=1` | Moonlit night |
| `-gkpalette=2` | Rainstorm and lightning |
| `-gkpalette=3` | Warm dusty haze |
| `-gkpalette=4` | Toxic green |
| `-gkpalette=5` | Blue-pink twilight |

The preview also accepts `-gkclassiclook` for comparison. For scripted capture,
use `-gkscript` with `Saved/script.txt`. The script command `palette|0` through
`palette|5` selects a palette and `palette|-1` restores gameplay control.
Screenshots go to the project's `Saved/shots`.

The forest uses directional cel shading on the two armored fighters and keeps their
existing outline overlay. The angular HUD uses portraits captured from those Unreal
fighter meshes. The forest post-process bands the apparent illumination on nearby scenery. The environment
keeps its original textures, lights and fog components. This path uses the existing
Metal SM6 renderer. Unreal 5.8's experimental Substrate Toon BSDF requires a
different material path and was left disabled.

The editor's Python file watcher is opt-in. Launch the editor with
`GK_EDITOR_WATCH=1` only when you need the watcher for a tool. Run material
generation in a fresh Python commandlet. Avoid recompiling materials or replacing
referenced assets from a Slate tick callback during an editing session.
