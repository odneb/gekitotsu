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


## Authored Robot 1 animation pass

The selected performances and source timing windows are in
`Source/GekitotsuNative/GkAuthoredClips.h`. These are licensed assets installed locally;
the raw purchased packs are not distributed in this repository.

Add Mega Taunt and Mega Emotes through Fab to supply `/Game/TauntBundle` and
`/Game/EmotePack`. For the FBX packs, extract their files into `AnimeMartialArts`,
`AnimePowerUp`, and `SuperheroGround` folders under a common directory. Set
`GK_ANIMATION_SOURCE` to that directory and run `tools/import_authored_animations.py`
with Unreal's Python editor commandlet. The script imports missing clips only onto
the project's Manny skeleton, under `/Game/GkNewAnimations`, and writes a result
manifest under Saved. Existing assets are preserved. The old Unarmed V4 pack supplies
the paired Grab, Throw1 and Throw2 performances. Missing optional assignments keep
the established animation fallback; a complete local install logs 39/39 selections.

The two-bar SA2 is now a strike-confirm grab-bomb. Back + Heavy + Special activates
it with simple controls; a blocked or parried attempt does not latch. Successful
hits hold the pair on a shared clock and detonate after 90 unfrozen frames. The
blast follows combo scaling, simple-input penalty, burnout and other damage modifiers.
This version uses an adapted paired clinch, not a custom-authored back-cling animation.
Directional normal throws retain the tech window and release at frame 42; recovery
ends at frame 68. Their paired render spacing is 150 cm for this skeleton.

`tools/run_film_checks.sh` covers controls, beams, every move recovering, the bomb,
and directional throws for both players, facings and walls. `-gkscript` supports
`pair|attacker asset|victim asset|source frame at 60 Hz|spacing cm` for paired auditions
and `record|frame count|output folder|P1 input mask|P2 input mask` for native HUD-on
frame exports. Run the full motion and contact checks after changing a selected clip.


## Animation setup and visual presentation

Press **F6** in gameplay to open the animation editor. Choose a move/state from the
single dropdown, search the animation list or choose a pack, then **Test move**.
**Esc** or the on-screen **Back to animations** button returns to the same preview.
**F5** tests/retries, **Save animation** persists the assignment, and **Done/F6**
returns to play. Timing options are collapsed by default. Personal choices remain
in `Content/Gk/picks.txt`; do not overwrite that file during an update.

`GkPresentationDirector.h` and `Content/Gk/cinematography.txt` coordinate brief
impact treatments, character rim, stage dimming, lens punctuation and audio ducking.
Drive Impact has a sustained startup cue, asymmetric magenta/cyan brush wake,
separate armor/contact bursts and painted afterimages. `GkStyleFx.cpp` also traces
active hand/foot motion and adds directional dash/homing wakes. Ground contacts use
small masked brush curls and ballistic debris alongside the existing FluidNinja
pressure response. These changes do not alter hitboxes, inputs, damage or timing.

`M_GkInkStroke` is an original masked/unlit instanced material generated by
`tools/make_ink_stroke.py`. It uses the project's existing Metal SM6 path and
requires no additional plugin or engine patch. Effect lifetimes and instance pools
are bounded. Restart clears lingering particles/afterimages. Reduced flash and
camera settings continue to apply to the presentation director.

The opening match sequence flies through Dreadmarsh, follows both walk-ins and
voice poses, then dollies out. **Start/Enter** skips it; training bypasses it.
`tools/capture_entrance.txt`, `tools/check_workshop.txt` and
`tools/capture_visual_polish.txt` are native reproduction scripts. Launch with
`-gkscript -gkscriptfile=/absolute/path/to/script.txt`; add
`-UseFixedTimeStep -FPS=60` for frame export. Recorded PNGs are in `Saved/shots`.


## Forest rendering pass — 2026-11-14

The forest cel pass runs before temporal reconstruction; unlit fighters, sky and
energy are excluded from its opaque shading quantization. TSR, anisotropic texture
filtering, and restrained sharpening keep drawn edges readable. Depth of field,
motion blur and chromatic aberration are disabled, including cinematic shots.
Directional fighter rim and tiered highlights follow the stage lighting. The
original landscape receives slope-masked wet patches; foliage keeps its existing
wind and opacity masks. No waterfall, light-card geometry or terrain edits were
introduced by this pass.

FluidNinja LIVE2 supplies the actual density/velocity simulation and fighter
interaction. A local material derivative gives the vapor three light-value tiers.
Its separate translucency pass is named AfterDOF by Unreal, but DOF itself is off.
The 256x512 simulation and 128 ray samples favor appearance over frame rate.
The stage palettes continue to blend from gameplay state.

After installing the owned Dreadmarsh and FluidNinja packs, run:

```sh
zsh native/tools/prepare_render_assets.sh
```

Run from the repository root. Set GK_ENGINE_ROOT if Unreal is installed elsewhere.
This reconstructs local vendor-derived shaders excluded from Git, then regenerates
the original fighter/forest/sky materials. It does not edit vendor assets or personal
animation picks. Existing local installations already contain these derivatives.
Logs are under native/Saved/RenderSetup. configure_film_fog.py is an optional editor
utility for the isolated fog sublevel; run material preparation first.

Validated with the Mac Development editor target, film gameplay checks, six-palette
settling/transition capture, dash footage, full entrance and entrance skip tests.
At 1672x940 native resolution, observed frame times were roughly 31–35 ms on the
current Mac with another editor open. These are sampled editor-game measurements,
not a packaged 60 FPS certification. An 80% render-scale test improved frame time
but degraded vapor stability, so it is not the default. Unused imported ocean
materials still emit existing Metal shader warnings; the new look materials compile.

For visual tuning, gk.Look.* console variables expose rim, shadow thresholds, bloom,
sun direction, sky fill, cloud placement, wet coverage, and mist settings. Keep
picks.txt and the original Dreadmarsh level unchanged during look development.


### Current baseline: dash-video render plus restrained authored atmosphere

Restored the rendering baseline from 8b59c1a used for real-game-dash.mp4, including
FluidNinja vapor, reflective wet patches, original landscape normals, and environment
cel blend 0.92. The later posterization/matte-ground experiments are removed.
The one deliberate change from that video is atmosphere: preserve the map author's
fog position, falloff, scattering and both layer profiles, scaling both densities to
28% (gk.Look.FogScale=0.28). Other palettes still shift fog density and tint.
Personal animation picks are not part of this rollback.

Environment sun-facing contours now receive a narrow directional highlight (gk.Look.WorldRim=0.65, WorldRimWidth=0.12). Bloom diffusion scale is 0.6 with grain 0.02; depth of field remains disabled. Compared actual HUD-on captures at 20%, 35%, and 50% authored fog: the stronger variants washed the background yellow, so the final density is 28%.

Forest gameplay uses a restrained 46-degree horizontal lens (8% narrower than the stage’s authored 50 degrees) and 5% tighter base framing. The previous 18-degree / 70%-height experiment was rejected because it hid too much of the stage. Both-fighter horizontal safety remains unchanged.
