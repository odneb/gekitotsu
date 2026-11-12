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
