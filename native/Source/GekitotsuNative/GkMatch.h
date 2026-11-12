#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "InputCoreTypes.h"
#include "Core/gk_sim.h"
#include "Core/gk_ai.h"
#include "GkBody.h"
#include "GkFx.h"
#include "GkMatch.generated.h"

class UPoseableMeshComponent; class UStaticMeshComponent; class USceneComponent; class ACameraActor; class AExponentialHeightFog; class UCanvas; class FGkPads;

/** A place to fight. Kind 0 is the measured training room and 1 the canyon, both built by the game on its own bare map; 2 is a dressed map from an environment pack, opened as it was shipped, with the fight set down in it. */
struct FGkStage { FString Name, Map, Sky; int32 Kind = 2; double Time = 500.0; bool bFixed = false; FVector At = FVector::ZeroVector; double Yaw = 0.0; };
/** What has been chosen in the menus. It outlives a change of map. Char: 0 is Noa, 1 and 2 the mannequins. */
struct FGkSetup { int32 Stage = 1, Mode = 0, Level = 2, Dummy = 0, Char[2] = { 0, 0 }; bool bGo = false; };

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
	static FGkSetup Setup; static TArray<FGkStage> Stages; static void LoadStages();
	// where the fight stands in this map, and whether that has been settled yet (a pack's map streams in around the camera first)
	FTransform Anchor = FTransform::Identity; int32 Kind = 1; bool bPlaced = true; float PlaceWait = 0.f; FString PlaceNote;
	FVector Wd(const FVector& Local) const { return Anchor.TransformPosition(Local); }
	// the menus: 0 the front menu, 1 choosing fighters, 2 choosing a stage, 3 the fight (bMenu is then its pause menu)
	int32 Screen = 0, SelWho = 0, StageAt = 1, LightsClock = 0;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FloorSlab;
	void DressFighter(int32 i); void StartStage(int32 Index); bool TryPlace(); void QuietMapLights(); FString MapName() const;

	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Body[2];
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ShotMesh[4];
	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	// the two places to fight: the measured training room, and the wasteland stage; and the air between
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> GridParts;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> StageParts;
	UPROPERTY() TObjectPtr<AExponentialHeightFog> Fog;
	FGkFx Fx; int32 ShownPlace = -1;
	// The sky and weather over the wasteland (SoStylized's Stylized Dynamic Sky & Weather): it owns the sun, sky light and
	// fog there. The plain lights below are the training room's.
	UPROPERTY() TObjectPtr<AActor> SkyActor;
	UPROPERTY() TArray<TObjectPtr<UObject>> Weathers;
	UPROPERTY() TArray<TObjectPtr<AActor>> RoomLights;
	UPROPERTY() TObjectPtr<USceneComponent> RoomAir;
	float Storm = 0.f, Dim = 0.f; int32 WeatherAt = -1;
	void MakeSky(); void SetWeather(int32 Index, double BlendSeconds);
	// the browser game's animation and the two skeletons it is solved onto
	FGkPoses Poses; FGkBody Rig[2]; TArray<FName> MoveClip; int32 HitKind[2] = { 1, 1 };

	// the match
	gk::State S; gk::Ai Cpu[2];
	int32 Mode = 0, Level = 2, Dummy = 0;            // mode: 0 versus CPU, 1 training, 2 watch. dummy: stand, crouch, jump, block all, CPU
	double Acc = 0.0, LastReal = 0.0; float FrameMs = 16.7f, StepMs = 0.f; int32 OverTimer = 0, Ticks = 0;
	// what the picture adds on top of the rules
	float Shake = 0.f; FVector CamPos = FVector(-900.0, 0.0, 120.0);
	// The camera's director, as in the browser game: it sits side-on while the fight is being read and swings round the
	// action for the moments that deserve it. Yaw orbits the point looked at, Pit is elevation, Roll the dutch tilt, Zm
	// the distance, Fov a widening; the K values are one-shot shoves from impacts that die away on their own.
	struct FCamDir { float Yaw = 0.f, Pit = 4.f, Roll = 0.f, Zm = 1.f, Fov = 0.f, KRoll = 0.f, KZm = 0.f, KFov = 0.f, KYaw = 0.f; bool bSnap = true; } CamD; int32 FrzMax = 0;
	void CamKick(float Roll, float Zm, float Fov, float Yaw = 0.f);
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
