// The combat core is plain C++ with no engine dependency. It is compiled here as one unit, with the engine's
// stricter warnings relaxed for it, so the same source also builds and runs its tests outside Unreal.
#include "CoreMinimal.h"
THIRD_PARTY_INCLUDES_START
#include "Core/gk_core.inl"
THIRD_PARTY_INCLUDES_END
