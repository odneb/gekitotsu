#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "InputCoreTypes.h"
#include "Core/gk_sim.h"
#include "Core/gk_ai.h"
#include "GkBody.h"
#include "GkMatch.generated.h"

class UPoseableMeshComponent; class UStaticMeshComponent; class ACameraActor; class UCanvas; class FGkPads;

UCLASS()
class GEKITOTSUNATIVE_API AGkGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AGkGameMode();
	virtual void StartPlay() override;
};

UCLASS()
class GEKITOTSUNATIVE_API AGkHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};

/**
 * One match. The rules live in Core/ (gk::step): a fixed 60 Hz simulation that knows nothing of Unreal. This actor
 * feeds it the buttons, steps it in real time whatever the picture's frame rate, and shows the result: two skeletal
 * fighters whose animation is set to the exact frame the rules are on, the camera, and the HUD.
 */
UCLASS()
class GEKITOTSUNATIVE_API AGkMatch : public AActor
{
	GENERATED_BODY()
public:
	AGkMatch();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	void DrawHud(UCanvas* Cv);
	static AGkMatch* Current;

	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Body[2];
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ShotMesh[4];
	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	// the browser game's animation and the two skeletons it is solved onto
	FGkPoses Poses; FGkBody Rig[2]; TArray<FName> MoveClip; int32 HitKind[2] = { 1, 1 };

	// the match
	gk::State S; gk::Ai Cpu[2];
	int32 Mode = 0, Level = 2, Dummy = 0;            // mode: 0 versus CPU, 1 training, 2 watch. dummy: stand, crouch, jump, block all, CPU
	double Acc = 0.0, LastReal = 0.0; float FrameMs = 16.7f, StepMs = 0.f; int32 OverTimer = 0, Ticks = 0;
	// what the picture adds on top of the rules
	float Shake = 0.f; FVector CamPos = FVector(-900.0, 0.0, 120.0);
	FString CallWord; int32 CallAge = 0, CallHold = 0;
	int32 Combo[2] = { 0, 0 }, ComboDmg[2] = { 0, 0 }, ComboTimer[2] = { 0, 0 }; FString Note[2]; int32 NoteTimer[2] = { 0, 0 };
	struct FSpark { FVector At; int32 Age; int32 Kind; }; TArray<FSpark> Sparks;
	// training aids
	struct FHist { int32 In, Frames; }; TArray<FHist> Hist; int32 LastIn = -1, Adv = -999, ActFrame[2] = { 0, 0 }; bool bWasIdle[2] = { true, true }, bExchange = false; bool bBoxes = true;
	// controls
	FGkPads* Pads = nullptr; uint32 PadNow = 0, PadPrev = 0; int32 PadDirs = 0, PadDirPrev = 0, PadCount = 0; FString PadNames;
	int32 PadBind[8] = { 0, 1, 2, 6, 7, 5, 4, 3 }; FKey Bind[8]; int32 InputNow = 0;
	bool bMenu = true; int32 MenuAt = 0, MenuPage = 0, BindAt = 0;
	float ScreenPct = 0.f, SlowSeconds = 0.f; FString StatusPath; int32 StatusClock = 0;

	static constexpr double Cm = (100.0 / 51.0) / 100.0;   // centimetres in one of the rules' sub-units: the fighter is 89 units, about 1.75 m
	FVector At(int32 X, int32 Y, double Depth = 0.0) const { return FVector(Depth, double(X) * Cm, double(Y) * Cm); }
	void Restart(); int32 ReadPlayer() const; int32 DummyInput(); void MenuInput(); void StepOnce(); void OnEvent(const gk::Event& E); void Track(int32 In);
	void Present(float Real); void ShowFighter(int32 i); void SaveBinds() const; void LoadBinds(); void Say(const TCHAR* Word, int32 Hold); void Remark(int32 Who, const TCHAR* Text);
};
