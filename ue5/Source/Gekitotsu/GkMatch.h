#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "Core/gk_core.h"
#include "GkPose.h"
#include "GkMatch.generated.h"

class UPoseableMeshComponent;
class ACameraActor;

/** One fighter on screen. It owns no rules: every frame it is told the state of the simulation and poses itself to match. */
UCLASS()
class GEKITOTSU_API AGkFighter : public AActor
{
	GENERATED_BODY()
public:
	AGkFighter();
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Mesh;
	void Setup(int32 InIndex);
	void Present(const gk::State& S, int32 Frame);
private:
	int32 Index = 0; FGkRig Rig; FGkPose Cur; bool bHasPose = false; TArray<FTransform> Bones;
	FGkPose Target(const gk::State& S, float& OutSnap) const;
};

/** Owns one match: reads the controls, steps the combat core at a fixed 60 Hz, and directs the fighters and the camera. */
UCLASS()
class GEKITOTSU_API AGkMatch : public AActor
{
	GENERATED_BODY()
public:
	AGkMatch();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	gk::State State;
	int32 Combo[2] = { 0, 0 }; int32 ComboTimer[2] = { 0, 0 };
	float Shake = 0.f;

	UFUNCTION(BlueprintPure, Category = "Gekitotsu") FVector FighterLocation(int32 Index) const;
	UFUNCTION(BlueprintPure, Category = "Gekitotsu") int32 FighterHealth(int32 Index) const;

private:
	double Accumulator = 0.0; double IdleSeconds = 0.0; uint32 Rng = 2463534242u;
	UPROPERTY() TObjectPtr<AGkFighter> Fighters[2];
	UPROPERTY() TObjectPtr<ACameraActor> Camera;
	FVector CamPos = FVector(700, 0, 110);
	uint16 ReadPlayer() const;
	uint16 Cpu(int32 Me);
};

UCLASS()
class GEKITOTSU_API AGkHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};

UCLASS()
class GEKITOTSU_API AGkGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AGkGameMode();
	virtual void StartPlay() override;
};
