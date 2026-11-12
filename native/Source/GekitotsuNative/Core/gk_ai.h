// The CPU opponent: a translation of the browser game's (io.js). Rule-based; every choice is a pure function of the
// match state, so the same situation always produces the same play. It reads the state a player sees and returns buttons.
#pragma once
#include "gk_sim.h"

namespace gk {

struct Ai { int q[256]; int qn = 0, qh = 0, level = 2, hold = 0, cool = 0; };
void aiMake(Ai& ai, int level);                 // level 1 to 3
int aiThink(Ai& ai, const State& s, int me);    // the buttons this side presses this frame

}  // namespace gk
