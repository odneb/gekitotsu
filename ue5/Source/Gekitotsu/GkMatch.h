#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/gk_core.h"
#include "GkMatch.generated.h"

/** Owns one match. Steps the combat core at a fixed 60 Hz no matter what the render frame rate is. */
UCLASS()
class GEKITOTSU_API AGkMatch : public AActor
{
	GENERATED_BODY()
public:
	AGkMatch();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Input bits for each player for the next simulation step (see gk_core.h). */
	uint16 Inputs[2] = { 0, 0 };
	gk::State State;

	UFUNCTION(BlueprintPure, Category = "Gekitotsu") FVector FighterLocation(int32 Index) const;
	UFUNCTION(BlueprintPure, Category = "Gekitotsu") int32 FighterHealth(int32 Index) const;

private:
	double Accumulator = 0.0;
};
