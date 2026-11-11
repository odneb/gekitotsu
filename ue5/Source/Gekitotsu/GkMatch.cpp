#include "GkMatch.h"

AGkMatch::AGkMatch() { PrimaryActorTick.bCanEverTick = true; }

void AGkMatch::BeginPlay() { Super::BeginPlay(); gk::init(State); Accumulator = 0.0; }

void AGkMatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Accumulator += FMath::Min(DeltaSeconds, 0.1f);
	const double Step = 1.0 / 60.0;
	while (Accumulator >= Step) { gk::step(State, Inputs[0], Inputs[1]); Accumulator -= Step; }
}

FVector AGkMatch::FighterLocation(int32 Index) const
{
	const gk::Player& P = State.p[Index & 1];
	// the core's line of play is the world Y axis; its units are hundredths of a centimetre
	return FVector(0.0, -double(P.x) / gk::F, double(P.y) / gk::F);
}

int32 AGkMatch::FighterHealth(int32 Index) const { return State.p[Index & 1].hp; }
