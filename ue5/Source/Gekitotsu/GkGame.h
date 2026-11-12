#pragma once
#include "CoreMinimal.h"
#include "Core/gk_core.h"
#include "GkPose.h"

class AGkMatch; class UCanvas; class USkeletalMesh; class ACameraActor; class UWorld;
class UPoseableMeshComponent; class UStaticMeshComponent; class UMaterialInstanceDynamic; class UMaterialInterface;
class ULightComponent; class USkyLightComponent; class UExponentialHeightFogComponent; class UPointLightComponent;

/** One fighter on screen. He owns no rules: each frame he is shown the state of the simulation and poses to match. */
struct FGkView
{
	UPoseableMeshComponent* Mesh = nullptr; UMaterialInstanceDynamic* BodyMat = nullptr;
	FGkPose Cur; bool bHasPose = false; TArray<FTransform> Bones;
	FName Frozen; int32 FrozenTime = 0;              // a clip and time held for inspection
	int32 Drawing = -1;                              // the baked frame on show
	UPoseableMeshComponent* Ghost[4] = { nullptr, nullptr, nullptr, nullptr };
	UMaterialInstanceDynamic* GhostMat[4] = { nullptr, nullptr, nullptr, nullptr };
	float GhostAge[4] = { 99.f, 99.f, 99.f, 99.f }; int32 GhostNext = 0;
	float HpLag = 1.f; int32 Flash = 0, GhostClock = 0;
};

/** One flat effect shape facing the camera: a spark, ring, streak, crescent or flame, drawn by the M_Fx material. */
struct FGkFx
{
	UStaticMeshComponent* Quad = nullptr; UMaterialInstanceDynamic* Mat = nullptr;
	bool bOn = false; int32 Kind = 0; float Age = 0.f, Life = 1.f;
	FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector;
	float Size0 = 100.f, Size1 = 100.f, Stretch = 1.f, Rot = 0.f, Alpha = 1.f, Seed = 0.f;
	FLinearColor Col = FLinearColor::White, Core = FLinearColor::White;
};

/** A whole match and how it is shown: the simulation, the fighters, effects, camera, the mood of the world, the HUD. */
struct FGkGame
{
	static FGkGame* Current;
	AGkMatch* Owner = nullptr; UWorld* World = nullptr;
	gk::State S; double Acc = 0.0, IdleSeconds = 0.0; uint32 Rng = 2463534242u; int32 Tick60 = 0;
	FGkRig Rig; FGkView V[2]; USkeletalMesh* SkMesh = nullptr; UMaterialInterface* InkMat = nullptr;
	int32 PrevMv[2] = { 0, 0 }, PrevMf[2] = { 0, 0 }; bool bSheet = false;
	TArray<FGkFx> Fx; TArray<UPoseableMeshComponent*> Sheet;
	UStaticMeshComponent* ShotQuad[2][2] = { { nullptr, nullptr }, { nullptr, nullptr } }; UMaterialInstanceDynamic* ShotMat[2][2] = { { nullptr, nullptr }, { nullptr, nullptr } };
	UStaticMeshComponent* SheetQuad[3] = { nullptr, nullptr, nullptr }; UMaterialInstanceDynamic* SheetMat[3] = { nullptr, nullptr, nullptr };
	UPointLightComponent* HitLight = nullptr; UPointLightComponent* ShotLight[2] = { nullptr, nullptr };
	ACameraActor* Cam = nullptr; FVector CamPos = FVector(1000, 0, 110);
	ULightComponent* Sun = nullptr; ULightComponent* Fill = nullptr; USkyLightComponent* Sky = nullptr; UExponentialHeightFogComponent* Fog = nullptr;
	UMaterialInstanceDynamic* SkyMat = nullptr; UMaterialInstanceDynamic* SunMat = nullptr;
	// direction: how the world and the lens react to the fight
	float Shake = 0.f, Punch = 0.f, Storm = 0.f, Dim = 0.f, Flash = 0.f, White = 0.f, HitGlow = 0.f; FVector HitAt = FVector::ZeroVector; FLinearColor HitCol = FLinearColor::White;
	int32 Mono = 0, SlowFrames = 0;
	float FrameMs = 16.f, StepMs = 0.f, ShowMs = 0.f; double LastReal = 0.0;
	int32 Combo[2] = { 0, 0 }, ComboTimer[2] = { 0, 0 }, ComboPop[2] = { 0, 0 };
	int32 SuperWho = -1, SuperLevel = 0, SuperTimer = 0, OverTimer = 0;
	// testing
	bool bPaused = false; int32 StepsOwed = 0; int32 ScriptIn[2] = { 0, 0 }, ScriptFrames[2] = { 0, 0 }; bool bCpu[2] = { true, true }; bool bHud = true;
	TMap<FName, float> Tune;

	~FGkGame();
	void Begin(AGkMatch* InOwner);
	void SetLumen(bool bOn);
	void Tick(float DeltaSeconds);
	FString Debug(const FString& Command);
	void DrawHud(UCanvas* Cv);

	float Tn(const TCHAR* Key, float Default) const { const float* F = Tune.Find(FName(Key)); return F ? *F : Default; }
	uint16 ReadPlayer() const;
	uint16 Cpu(int32 Me);
	void StepOnce();
	void OnEvent(const gk::Event& E);
	void Pick(int32 Index, FName& Clip, int32& Time, float& Roll, float& Spin) const;
	void Pose(UPoseableMeshComponent* Mesh, int32 FrameIndex, bool bMirror, TArray<FTransform>& Bones) const;
	void ShowFighter(int32 Index, int32 Steps);
	void ShowWorld(float DeltaSeconds);
	FGkFx& Spawn(int32 Kind, const FVector& Pos, float Size, float Life, const FLinearColor& Col, const FLinearColor& Core);
	void TickFx(int32 Steps);
	FVector At(int32 X, int32 Y, double Depth = 30.0) const { return FVector(Depth, -double(X) / gk::F, double(Y) / gk::F); }
	FLinearColor Hue(int32 Who) const; FLinearColor HueCore(int32 Who) const;
	float Rand() { Rng ^= Rng << 13; Rng ^= Rng >> 17; Rng ^= Rng << 5; return (Rng & 0xffff) / 65535.f; }
};
