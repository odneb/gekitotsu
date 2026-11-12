// The combat simulation and the CPU opponent are plain C++ with no engine dependency (Core/). They are compiled here
// as one unit, so the very same source also builds outside Unreal, where tools/run_checks.sh plays it against the
// browser game's rules frame by frame.
#include "CoreMinimal.h"
THIRD_PARTY_INCLUDES_START
#include "Core/gk_sim.inl"
#include "Core/gk_ai.inl"
THIRD_PARTY_INCLUDES_END
