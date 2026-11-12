#pragma once
#include "CoreMinimal.h"

class UInstancedStaticMeshComponent; class AActor; class USceneComponent;

/**
 * The fight's particles: hit and guard sparks, dust, ki, and the motes and haze that hang in the air. Each particle is
 * a flat square that faces the camera (the camera always looks straight down the stage), drawn as one instance of an
 * instanced mesh; there are two layers, a glow that adds light and a puff that veils what is behind it.
 */
struct FGkFx
{
	struct FP { FVector Pos, Vel; float Age, Life, Size0, Size1, Stretch, Grav, Drag, Alpha; FLinearColor Col; int32 Layer; bool bSwell; };
	TArray<FP> Ps; UInstancedStaticMeshComponent* Mesh[2] = { nullptr, nullptr }; int32 Used[2] = { 0, 0 }; FRandomStream Rng; float MoteDue = 0.f, HazeDue = 0.f;
	FLinearColor DustCol = FLinearColor(0.62f, 0.47f, 0.33f);
	static constexpr int32 Pool = 420;

	void Init(AActor* Owner, USceneComponent* Root);
	void Tick(float Dt);
	FP& Add(int32 Layer, const FVector& Pos, const FVector& Vel, float Life, float Size0, float Size1, const FLinearColor& Col, float Alpha = 1.f);
	/** Level 0 light, 1 medium, 2 heavy, 3 a finishing blow. Dir is the way the blow travels along the stage (+1 or -1). */
	void Hit(const FVector& At, int32 Level, float Dir);
	void Guard(const FVector& At, float Dir);
	void Parry(const FVector& At, bool bPerfect);
	void Dust(const FVector& At, float Amount, float Dir);
	void Aura(const FVector& At, const FLinearColor& Col, float Amount);
	void Trail(const FVector& At, const FLinearColor& Col);
	/** Motes and ground haze around the part of the stage in view. */
	void Air(double CentreY, float Dt, bool bOutdoors);
};
