#include "GkMatch.h"
#include "GkPad.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/LightComponent.h"
#include "Components/LightComponentBase.h"
#include "UObject/UnrealType.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"
#include "Components/VolumetricCloudComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Components/SpotLightComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"

AGkMatch* AGkMatch::Current = nullptr;
static FString GkSkyForce; static double GkTimeForce = -1.0;   // a script trying a stage under another sky
static double GkFovBase = 30.0;   // the fight lens, in degrees across: a stage may ask for a wider one
static bool GkFlyOn() { static int32 On = -1; if (On < 0) { On = FPaths::FileExists(*(FPaths::ProjectSavedDir() / TEXT("fly.on"))) ? 1 : 0; } return On == 1; }   // a note left in Saved asks for a free camera on a stage
/** Turns the map's sun to shine back at the camera, so the fight is lit from behind and its edges and the haze catch the light. Yaw and pitch are in degrees; Mult scales the sun's own strength. */
static void GkApplySun(UWorld* W, double Yaw, double Pitch, double Mult, const FLinearColor& Col, bool bCol)
{
	if (!W) { return; }
	static TMap<TWeakObjectPtr<UObject>, float> Orig;
	for (TActorIterator<ADirectionalLight> It(W); It; ++It)
	{
		UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(It->GetLightComponent()); if (!D) { continue; }
		float& O = Orig.FindOrAdd(TWeakObjectPtr<UObject>(D), D->Intensity);
		D->SetIntensity(O * float(Mult)); It->SetActorRotation(FRotator(Pitch, Yaw, 0.0)); if (bCol) { D->SetLightColor(Col.ToFColor(true)); }
	}
}
/** How the scene is lighting the fighters right now: the brightest sun or moon's colour, scaled by how strong it is and how high it stands, over a small ambient floor. 1 = as bright as the base colours were drawn for. */
static float GkLitK = 1.f;
static FLinearColor GkSceneTint(UWorld* W, float& Dir)
{
	Dir = 0.f; if (!W || GkLitK <= 0.f) { return FLinearColor::White; }
	float Best = 0.f; FLinearColor Col = FLinearColor::White;
	for (TActorIterator<ADirectionalLight> It(W); It; ++It)
	{
		UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(It->GetLightComponent()); if (!D || !D->IsVisible()) { continue; }
		const float Elev = -It->GetActorRotation().Pitch; const float Up = FMath::SmoothStep(-6.f, 22.f, Elev);
		const float B = FMath::Clamp(D->Intensity / 7.f, 0.f, 1.4f) * (0.3f + 0.7f * Up);
		if (B > Best) { Best = B; Col = FLinearColor(D->GetLightColor()); }
	}
	const float Amb = 0.5f; const float Lv = FMath::Clamp(Amb + Best, 0.f, 1.35f);
	Dir = Best;
	const FLinearColor Warm = FLinearColor::LerpUsingHSV(FLinearColor::White, Col, 0.55f);
	const FLinearColor T = Warm * Lv;
	return FMath::Lerp(FLinearColor::White, FLinearColor(T.R, T.G, T.B, 1.f), FMath::Clamp(GkLitK, 0.f, 1.f));
}
/** The crisp edge light: numbers as a stage or the script gives them (thickness, softness, strength, red, green, blue as hundredths; near and far fade in cm; sharpness in hundredths). */
static void GkEdge(UMaterialInstanceDynamic* M, const TArray<FString>& N)
{
	if (!M) { return; }
	auto V = [&N](int32 k, float Def) { return N.IsValidIndex(k) && !N[k].IsEmpty() ? FCString::Atof(*N[k]) : Def; };
	if (N.Num() == 1) { M->SetScalarParameterValue(TEXT("Strength"), V(0, 100.f) / 100.f); return; }
	M->SetScalarParameterValue(TEXT("Thr"), V(0, 55.f) / 100.f); M->SetScalarParameterValue(TEXT("Soft"), V(1, 4.f) / 100.f); M->SetScalarParameterValue(TEXT("Strength"), V(2, 100.f) / 100.f);
	M->SetVectorParameterValue(TEXT("Color"), FLinearColor(V(3, 55.f) / 100.f, V(4, 100.f) / 100.f, V(5, 90.f) / 100.f));
	M->SetScalarParameterValue(TEXT("Near"), V(6, 2500.f)); M->SetScalarParameterValue(TEXT("Far"), V(7, 9000.f)); M->SetScalarParameterValue(TEXT("Exp"), V(8, 200.f) / 100.f);
}
static int32 GkScriptResume = 0;   // a script that changes stage carries on from here in the new map
FGkSetup AGkMatch::Setup; TArray<FGkStage> AGkMatch::Stages;

/** The stages: the two the game builds itself, then one for each line of Content/Gk/stages.txt (name | map | sky preset | time | "auto" or x,y,z,yaw). */
void AGkMatch::LoadStages()
{
	if (Stages.Num() > 0) { return; }
	{ FGkStage G; G.Name = TEXT("TRAINING ROOM"); G.Map = TEXT("/Engine/Maps/Entry"); G.Kind = 0; Stages.Add(G); }
	{ FGkStage G; G.Name = TEXT("DESERT CANYON"); G.Map = TEXT("/Game/GkStage/L_GkCanyonTerrain"); G.Kind = 1; G.Sky = TEXT("Desert"); G.Time = 500.0; G.Opt = TEXT("rim=1.0,0.62,0.3;air=sand"); Stages.Add(G); }
	FString Text; if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Gk/stages.txt")))) { return; }
	TArray<FString> Lines; Text.ParseIntoArrayLines(Lines);
	for (const FString& Ln : Lines)
	{
		TArray<FString> T; Ln.ParseIntoArray(T, TEXT("|")); if (T.Num() < 4 || Ln.StartsWith(TEXT("#"))) { continue; }
		FGkStage G; G.Name = T[0].TrimStartAndEnd().ToUpper(); G.Map = T[1].TrimStartAndEnd(); G.Sky = T[2].TrimStartAndEnd(); G.Time = FCString::Atod(*T[3]); G.Kind = 2;
		if (T.Num() > 4) { TArray<FString> N; T[4].ParseIntoArray(N, TEXT(",")); if (N.Num() >= 4) { G.bFixed = true; G.At = FVector(FCString::Atod(*N[0]), FCString::Atod(*N[1]), FCString::Atod(*N[2])); G.Yaw = FCString::Atod(*N[3]); } }
		if (T.Num() > 5) { G.Opt = T[5].TrimStartAndEnd(); }
		Stages.Add(G);
	}
}
FString AGkMatch::MapName() const { FString N = GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString(); return UWorld::RemovePIEPrefix(N); }

AGkGameMode::AGkGameMode() { HUDClass = AGkHUD::StaticClass(); DefaultPawnClass = nullptr; }
void AGkGameMode::StartPlay()
{
	Super::StartPlay();
	if (UWorld* W = GetWorld()) { W->SpawnActor<AGkMatch>(FVector::ZeroVector, FRotator::ZeroRotator); }
}
void AGkHUD::DrawHUD() { Super::DrawHUD(); if (AGkMatch::Current) { AGkMatch::Current->DrawHud(Canvas); } }

AGkMatch::AGkMatch()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}


// ------------------------------------------------------------------------------------------------ setting up
void AGkMatch::BeginPlay()
{
	Super::BeginPlay();
	Current = this; UWorld* W = GetWorld();
	// a standard pad as Unreal names its buttons, in the browser game's layout: Square Light, Cross Medium, Circle Heavy, L2 Special, R2 Assist, R1 Parry, L1 Impact, Triangle Homing
	Bind[0] = EKeys::Gamepad_FaceButton_Left; Bind[1] = EKeys::Gamepad_FaceButton_Bottom; Bind[2] = EKeys::Gamepad_FaceButton_Right; Bind[3] = EKeys::Gamepad_LeftTrigger;
	Bind[4] = EKeys::Gamepad_RightTrigger; Bind[5] = EKeys::Gamepad_RightShoulder; Bind[6] = EKeys::Gamepad_LeftShoulder; Bind[7] = EKeys::Gamepad_FaceButton_Top;
	LoadBinds();
	Pads = new FGkPads();
	// which stage this is: the one chosen in the menus if this is its map, otherwise whatever this map can be
	LoadStages();
	{
		const FString Here = MapName(); int32 Is = Stages.IsValidIndex(Setup.Stage) && Stages[Setup.Stage].Map == Here ? Setup.Stage : INDEX_NONE;
		if (Is == INDEX_NONE) { for (int32 k = Stages.Num() - 1; k >= 0; k--) { if (Stages[k].Map == Here && (Stages[k].Kind != 0 || Is == INDEX_NONE)) { Is = k; } } }
		if (Is == INDEX_NONE) { Is = 1; Kind = Here == TEXT("/Engine/Maps/Entry") ? 1 : 2; } else { Kind = Stages[Is].Kind; }
		Setup.Stage = Is; StageAt = Is; Mode = Setup.Mode; Level = Setup.Level; Dummy = Setup.Dummy;
		Screen = Setup.bGo ? 3 : 0; Setup.bGo = false; bMenu = false; Anchor = GetActorTransform(); bPlaced = Kind != 2;
		if (FParse::Param(FCommandLine::Get(), TEXT("gkfight"))) { Screen = 3; }
		if (FParse::Param(FCommandLine::Get(), TEXT("gkscript")) || GkFlyOn())
		{
			if (GkFlyOn()) { FString N; FFileHelper::LoadFileToString(N, *(FPaths::ProjectSavedDir() / TEXT("fly.on"))); Script = { FString::Printf(TEXT("stage|%d"), FCString::Atoi(*N)), TEXT("wait|400"), TEXT("fight|1"), TEXT("hud|0"), TEXT("storm|0") }; }
			else { FFileHelper::LoadFileToStringArray(Script, *(FPaths::ProjectSavedDir() / TEXT("script.txt"))); }
			bScript = Script.Num() > 0; if (bScript) { Screen = 3; Mode = 1; ScriptAt = GkScriptResume; }
		}
	}
	StageLook();
	Restart();
	FParse::Value(FCommandLine::Get(), TEXT("gkstatus="), StatusPath);

	// a neutral place to fight: a floor, a marker at each end of the stage, daylight
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	TArray<TObjectPtr<USceneComponent>>* Into = &GridParts;
	auto Piece = [this, &Into](UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this); C->SetMobility(EComponentMobility::Movable); C->SetupAttachment(RootComponent);
		if (Mesh) { C->SetStaticMesh(Mesh); } C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent(); C->SetRelativeLocation(Loc); C->SetRelativeScale3D(Scale);
		if (Into) { Into->Add(C); }
		return C;
	};
	const double Half = double(gk::STAGE) * Cm;
	UMaterialInterface* Flat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Kind != 2)
	{
	// A training room: a dark floor and back wall ruled with a measured grid, so every step, jump and knock-back can be
	// read against it. One square is 50 of the rules' units (about a metre); every second line is heavier. The gold line
	// is the middle of the stage and the red lines are its two ends.
	auto Paint = [Flat](UStaticMeshComponent* C, const FLinearColor& Col) { if (UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0, Flat)) { M->SetVectorParameterValue(TEXT("Color"), Col); } C->SetCastShadow(false); };
	const double WallX = 140.0, Sq = 50.0 * gk::F * Cm, Wide = 11.0 * Sq, High = 14.0 * Sq, Near = 700.0;
	const FLinearColor Slab(0.035f, 0.04f, 0.055f), Line(0.2f, 0.23f, 0.28f), Heavy(0.42f, 0.46f, 0.52f), Gold(0.9f, 0.6f, 0.1f), Red(0.8f, 0.1f, 0.08f);
	Paint(Piece(Plane, FVector(WallX - 1500.0, 0.0, -0.5), FVector(30.0, Wide * 2.0 / 100.0, 1.0)), Slab * 1.7f);
	{ UStaticMeshComponent* Wall = Piece(Plane, FVector(WallX, 0.0, High * 0.5), FVector(High / 100.0 + 20.0, Wide * 2.0 / 100.0, 1.0)); Wall->SetRelativeRotation(FRotator(90.0, 0.0, 0.0)); Paint(Wall, Slab); }
	auto Upright = [&](double Y, double Thick, const FLinearColor& Col)
	{
		Paint(Piece(Cube, FVector(WallX - 2.0, Y, High * 0.5), FVector(0.02, Thick, High / 100.0)), Col);                    // up the wall
		Paint(Piece(Cube, FVector(WallX - Near * 0.5, Y, 0.0), FVector(Near / 100.0, Thick, 0.02)), Col);                   // and across the floor toward the camera
	};
	for (int32 k = -10; k <= 10; k++) { if (k != 0) { Upright(k * Sq, k % 2 == 0 ? 0.04 : 0.02, k % 2 == 0 ? Heavy : Line); } }
	Upright(0.0, 0.06, Gold); Upright(Half, 0.07, Red); Upright(-Half, 0.07, Red);
	for (int32 k = 1; k <= 13; k++) { Paint(Piece(Cube, FVector(WallX - 2.0, 0.0, k * Sq), FVector(0.02, Wide * 2.0 / 100.0, k % 2 == 0 ? 0.04 : 0.02)), k % 2 == 0 ? Heavy : Line); }
	for (int32 k = 0; k <= 7; k++) { Paint(Piece(Cube, FVector(WallX - k * Sq, 0.0, 0.0), FVector(k == 0 ? 0.06 : 0.02, Wide * 2.0 / 100.0, 0.02)), k == 0 ? Heavy : Line); }
	}
	// The stage: a wasteland of red rock under a low sun. A sand floor, boulders close behind the fighters, mesas and
	// spires in the middle distance and great tablelands far off, each layer paler in the haze than the one before.
	Into = &StageParts;
	if (Kind != 2)
	{
		UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
		auto Lit = [Flat](UStaticMeshComponent* C, const FLinearColor& Col, bool bShadow) { if (UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0, Flat)) { M->SetVectorParameterValue(TEXT("Color"), Col); } C->SetCastShadow(bShadow); };
		const FLinearColor Sand(0.62f, 0.36f, 0.17f), Rock(0.44f, 0.19f, 0.10f), Deep(0.27f, 0.11f, 0.07f);
		FRandomStream R(4411);
		auto Shade = [&R, Rock, Deep]() { return FMath::Lerp(Deep, Rock, R.FRandRange(0.35f, 1.f)); };
		auto Mesa = [&](double X, double Y, double W, double H, double D)
		{
			UStaticMeshComponent* Base = Piece(Cube, FVector(X, Y, H * 0.5), FVector(D / 100.0, W / 100.0, H / 100.0)); Base->SetRelativeRotation(FRotator(0.0, R.FRandRange(-14.f, 14.f), R.FRandRange(-3.f, 3.f))); Lit(Base, Shade(), true);
			UStaticMeshComponent* Top = Piece(Cube, FVector(X, Y + W * R.FRandRange(-0.15f, 0.15f), H * 1.1), FVector(D * 0.8 / 100.0, W * R.FRandRange(0.5f, 0.8f) / 100.0, H * 0.28 / 100.0)); Top->SetRelativeRotation(FRotator(0.0, R.FRandRange(-20.f, 20.f), 0.0)); Lit(Top, Shade(), true);
			Lit(Piece(Cone, FVector(X - D * 0.2, Y, H * 0.14), FVector(D * 1.7 / 100.0, W * 1.5 / 100.0, H * 0.3 / 100.0)), FMath::Lerp(Sand, Rock, 0.5f), false);
		};
		// The rock is GF Stylized's Desert Canyon set: small outcrops just behind the fight, cliffs in the middle distance,
		// tall buttes beyond them and two mountains on the horizon. (If the set is missing, plain blocks stand in.)
		auto Cliff = [&](const TCHAR* Name, double X, double Y, double Scale, bool bShadow)
		{
			const FString Path = FString::Printf(TEXT("/Game/GF_Desert_Canyon/Assets/%s"), Name);
			UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path);
			if (!M) { Mesa(X, Y, 700.0 * Scale, 500.0 * Scale, 600.0 * Scale); return; }
			UStaticMeshComponent* C = Piece(M, FVector(X, Y, -6.0 * Scale), FVector(Scale)); C->SetRelativeRotation(FRotator(0.0, R.FRandRange(0.f, 360.f), 0.0)); C->SetCastShadow(bShadow);
			// drawn the way the fighters are: the cel shader's lit variant over the rock's own painted colour (tools/make_stage_materials.py)
			if (UMaterialInterface* Cel = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/GkStage/MI_Cel_%s.MI_Cel_%s"), *M->GetName(), *M->GetName()))) { C->SetMaterial(0, Cel); }
		};
		static const TCHAR* Small[] = { TEXT("Cliffs/Cliff_06/SM_Cliff_6.SM_Cliff_6"), TEXT("Cliffs/Cliff_02/SM_Cliff_2.SM_Cliff_2"), TEXT("Cliffs/Cliff_08/SM_Cliff_8.SM_Cliff_8") };
		static const TCHAR* Mid[] = { TEXT("Cliffs/Cliff_10/SM_Cliff_10.SM_Cliff_10"), TEXT("Cliffs/Cliff_16/SM_CLIFF_16.SM_CLIFF_16"), TEXT("Cliffs/Cliff_01/SM_CLIFF_1.SM_CLIFF_1"), TEXT("Cliffs/Cliff_07/SM_Cliff_7.SM_Cliff_7"), TEXT("Cliffs/Cliff_11/SM_Cliff_11.SM_Cliff_11"), TEXT("Cliffs/Cliff_02/SM_Cliff_2.SM_Cliff_2"), TEXT("Cliffs/Cliff_08/SM_Cliff_8.SM_Cliff_8") };
		static const TCHAR* Tall[] = { TEXT("Cliffs/Cliff_03/SM_Cliff_3.SM_Cliff_3"), TEXT("Cliffs/Cliff_04/SM_Cliff_4.SM_Cliff_4"), TEXT("Cliffs/Cliff_05/SM_Cliff_5.SM_Cliff_5"), TEXT("Cliffs/Cliff_13/SM_Cliff_13.SM_Cliff_13"), TEXT("Cliffs/Cliff_15/SM_CLIFF_15.SM_CLIFF_15"), TEXT("Cliffs/Cliff_14/SM_CLIFF_14.SM_CLIFF_14"), TEXT("Cliffs/Cliff_09/SM_Cliff_9.SM_Cliff_9"), TEXT("Cliffs/Cliff_12/SM_Cliff_12.SM_Cliff_12") };
		for (int32 k = 0; k < 8; k++) { Cliff(Small[k % 3], R.FRandRange(380.f, 1000.f), -2400.0 + k * 640.0 + R.FRandRange(-200.f, 200.f), R.FRandRange(0.18f, 0.34f), true); }
		for (int32 k = 0; k < 9; k++) { Cliff(Mid[k % 7], R.FRandRange(2400.f, 5200.f), -5200.0 + k * 1300.0 + R.FRandRange(-400.f, 400.f), R.FRandRange(0.8f, 1.15f), true); }
		for (int32 k = 0; k < 9; k++) { Cliff(Tall[k % 8], R.FRandRange(7500.f, 13000.f), -10500.0 + k * 2600.0 + R.FRandRange(-800.f, 800.f), R.FRandRange(1.0f, 1.5f), false); }
		Cliff(TEXT("Mountain/Mountain_1/SM_Mountain_1.SM_Mountain_1"), 24000.0, -9500.0, 1.6, false);
		Cliff(TEXT("Mountain/Mountain_2/SM_Mountain_2.SM_Mountain_2"), 27000.0, 8500.0, 1.5, false);
	}
	Into = nullptr;
	Fx.Init(this, RootComponent);
	// the low sun seen through the haze behind everything
	if (Kind != 2) { Fx.Add(0, FVector(14000.0, 2500.0, 700.0), FVector::ZeroVector, -1.f, 9000.f, 9000.f, FLinearColor(1.0f, 0.55f, 0.25f), 0.5f); }
	for (int32 k = 0; k < 4; k++) { ShotMesh[k] = Piece(Ball, FVector::ZeroVector, FVector(0.45)); ShotMesh[k]->SetCastShadow(false); ShotMesh[k]->SetVisibility(false); }
	// a slab to stand on, for a pack's map whose ground is not level where the fight is set down
	FloorSlab = Piece(Cube, FVector(0.0, 0.0, -10.0), FVector(7.0, 24.0, 0.2)); FloorSlab->SetVisibility(false);
	if (UMaterialInstanceDynamic* SlabMat = FloorSlab->CreateDynamicMaterialInstance(0, Flat)) { SlabMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.22f, 0.2f, 0.19f)); }
	if (W && Kind != 2)
	{
		if (ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector(0.0, 0.0, 800.0), FRotator(-31.0, 38.0, 0.0)))
		{
			RoomLights.Add(Sun);
			if (ULightComponent* L = Sun->GetLightComponent()) { L->SetMobility(EComponentMobility::Movable); L->SetIntensity(8.f); L->SetLightColor(FLinearColor(1.f, 0.84f, 0.66f)); }
			if (UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(Sun->GetLightComponent())) { D->SetAtmosphereSunLight(true); D->SetDynamicShadowDistanceMovableLight(6000.f); }
		}
		Fog = W->SpawnActor<AExponentialHeightFog>(FVector(0.0, 0.0, 0.0), FRotator::ZeroRotator);
		if (Fog && Fog->GetComponent())
		{
			UExponentialHeightFogComponent* F = Fog->GetComponent(); F->SetMobility(EComponentMobility::Movable);
			F->SetFogDensity(0.022f); F->SetFogHeightFalloff(0.08f); F->SetFogInscatteringColor(FLinearColor(0.85f, 0.47f, 0.25f)); F->SetStartDistance(1200.f); F->SetFogMaxOpacity(0.92f);
		}
		USkyAtmosphereComponent* Air = NewObject<USkyAtmosphereComponent>(this); Air->SetupAttachment(RootComponent); Air->RegisterComponent(); RoomAir = Air;
		if (ASkyLight* Sky = W->SpawnActor<ASkyLight>(FVector(0.0, 0.0, 600.0), FRotator::ZeroRotator))
		{
			RoomLights.Add(Sky);
			if (USkyLightComponent* L = Sky->GetLightComponent()) { L->SetMobility(EComponentMobility::Movable); L->SetRealTimeCapture(true); L->SetIntensity(1.f); }
		}
	}

	// The fighters. The skeletons are Unreal's mannequins until the real characters are made; the animation is the browser
	// game's own, sampled for every move and state (Content/Gk/poses.*) and solved onto the skeleton each frame.
	if (!Poses.Load(FPaths::ProjectContentDir() / TEXT("Gk"))) { UE_LOG(LogTemp, Error, TEXT("Gekitotsu: the pose data (Content/Gk/poses.txt, poses.bin) did not load")); }
	MoveClip.Reset(); for (int32 k = 0; k < gk::moveCount(); k++) { MoveClip.Add(FName(*(FString(TEXT("mv_")) + UTF8_TO_TCHAR(gk::moveKey(k))))); }
	for (int32 i = 0; i < 2; i++)
	{
		UPoseableMeshComponent* C = NewObject<UPoseableMeshComponent>(this); C->SetupAttachment(RootComponent);
		C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent(); C->SetBoundsScale(30.f); C->bNeverDistanceCull = true;
		Body[i] = C;
		USkeletalMeshComponent* A = NewObject<USkeletalMeshComponent>(this); A->SetupAttachment(RootComponent);
		A->SetUsingAbsoluteLocation(true); A->SetUsingAbsoluteRotation(true); A->SetUsingAbsoluteScale(true); A->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		A->SetAnimationMode(EAnimationMode::AnimationSingleNode); A->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		A->RegisterComponent(); A->SetBoundsScale(30.f); A->bNeverDistanceCull = true; A->SetVisibility(false);
		Anim[i] = A;
	}
	LoadClips(); LoadVfx();
	for (int32 i = 0; i < 2; i++) { DressFighter(i); }
	if (W)
	{
		Cam = W->SpawnActor<ACameraActor>(CamPos, FRotator::ZeroRotator);
		if (Cam)
		{
			Cam->GetCameraComponent()->SetFieldOfView(30.f); Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
			// A fixed exposure: the cel shader's colours are painted values, and the eye adapting to a dark room washed them out.
			{ FPostProcessSettings& PP = Cam->GetCameraComponent()->PostProcessSettings;
			  PP.bOverride_AutoExposureMethod = true; PP.AutoExposureMethod = AEM_Manual;
			  PP.bOverride_AutoExposureBias = true; PP.AutoExposureBias = 0.f;
			  PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true; PP.AutoExposureApplyPhysicalCameraExposure = false;
			  PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 0.9f; PP.bOverride_BloomThreshold = true; PP.BloomThreshold = 1.0f;
			  PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.5f;
			  PP.bOverride_ColorSaturation = true; PP.ColorSaturation = FVector4(1.08, 1.08, 1.08, 1.0); PP.bOverride_ColorContrast = true; PP.ColorContrast = FVector4(1.06, 1.06, 1.06, 1.0);
			  if (ForestCel) { PP.AddBlendable(ForestCel, 1.f); }
			  Cam->GetCameraComponent()->PostProcessBlendWeight = 1.f; }
			if (APlayerController* PC = W->GetFirstPlayerController()) { PC->SetViewTarget(Cam); }
		}
	}
	MakeSky();
	// the lamp behind the fight: it lights the ground the fighters stand on from the far side and a little above, so they
	// stand out against it, and the mist between catches its beam
	{
		USpotLightComponent* L = NewObject<USpotLightComponent>(this); L->SetMobility(EComponentMobility::Movable); L->SetupAttachment(RootComponent);
		L->SetRelativeLocation(FVector(560.0, -380.0, 640.0)); L->SetRelativeRotation((FVector(0.0, 60.0, 40.0) - FVector(560.0, -380.0, 640.0)).Rotation());
		L->SetIntensityUnits(ELightUnits::Unitless); L->SetIntensity(LampI); L->SetLightColor(RimCol); L->SetAttenuationRadius(3200.f); L->SetOuterConeAngle(46.f); L->SetInnerConeAngle(24.f);
		L->SetCastShadows(true); L->SetVolumetricScatteringIntensity(LampScatter); L->RegisterComponent(); L->SetVisibility(Kind != 0 && LampI > 0.f); BackLamp = L;
	}
}

/** Reads how this stage wants to look from its options. */
void AGkMatch::StageLook()
{
	const FGkStage* St = Stages.IsValidIndex(Setup.Stage) ? &Stages[Setup.Stage] : nullptr; if (!St) { return; }
	bOwnLight = Kind == 2 && (GkSkyForce.IsEmpty() ? St->Get(TEXT("light")) == TEXT("map") : GkSkyForce == TEXT("map"));
	{ TArray<FString> N; St->Get(TEXT("rim")).ParseIntoArray(N, TEXT(",")); if (N.Num() >= 3) { RimCol = FLinearColor(FCString::Atof(*N[0]), FCString::Atof(*N[1]), FCString::Atof(*N[2])); } }
	static const TCHAR* Airs[8] = { TEXT("motes"), TEXT("petals"), TEXT("leaves"), TEXT("fireflies"), TEXT("snow"), TEXT("embers"), TEXT("spores"), TEXT("sand") };
	const FString Air = St->Get(TEXT("air")); AirKind = 0; for (int32 k = 0; k < 8; k++) { if (Air == Airs[k]) { AirKind = k; } }
	LampI = FCString::Atof(*St->Get(TEXT("lamp"), TEXT("5000"))); LensF = FCString::Atof(*St->Get(TEXT("lens"), TEXT("0")));
	bForestLook = St->Get(TEXT("look")) == TEXT("anime-forest") &&
		!FParse::Param(FCommandLine::Get(), TEXT("gkclassiclook"));
	if (bForestLook)
	{
		FParse::Value(FCommandLine::Get(), TEXT("-gkpalette="), PaletteOverride);
		PaletteOverride = FMath::Clamp(PaletteOverride, -1, 5);
		ForestFighterCel = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GkFx/M_GkFighterCel.M_GkFighterCel"));
		ForestCel = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GkFx/M_GkForestCel.M_GkForestCel"));
	}
	if (!bForestLook && !EdgeMid) { if (UMaterialInterface* EM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GkFx/M_GkRim.M_GkRim"))) { EdgeMid = UMaterialInstanceDynamic::Create(EM, this); } }
	{ TArray<FString> N; St->Get(TEXT("edge")).ParseIntoArray(N, TEXT(",")); if (N.Num() > 0) { GkEdge(EdgeMid, N); } } GkFovBase = FCString::Atod(*St->Get(TEXT("fov"), TEXT("30")));   // no soft focus unless a stage asks for it
}

/** FluidNinja LIVE's low fog, laid over the floor of the fight. It is a small level of its own (tools/make_ninja_fog.py takes
 *  the simulation out of FluidNinja's Fog_Mist level) streamed in at the fight's centre; four unseen balls follow the
 *  fighters' hips and feet, and it is those the fluid feels. */
void AGkMatch::NinjaFog()
{
	bNinjaTried = true; UWorld* W = GetWorld(); const FGkStage* St = Stages.IsValidIndex(Setup.Stage) ? &Stages[Setup.Stage] : nullptr;
	if (!W || (St && St->Get(TEXT("ninja")) == TEXT("0")) || FParse::Param(FCommandLine::Get(), TEXT("gknoninja"))) { return; }
	const bool bCanyonSand = Kind == 1;
	const TCHAR* Path = bCanyonSand ? TEXT("/Game/GkStage/L_GkNinjaSand") : TEXT("/Game/GkStage/L_GkNinjaMist");
	const FVector Offset = bCanyonSand ? FVector(0.0, 0.0, 8.0) : FVector(30.0, 0.0, 250.0);
	bool bOk = false;
	NinjaLevel = ULevelStreamingDynamic::LoadLevelInstance(W, FString(Path), Wd(Offset), Anchor.Rotator(), bOk);
	UE_LOG(LogTemp, Display, TEXT("GkNinja: %s %s"), Path, bOk ? TEXT("streaming in") : TEXT("did not load"));
	if (!bOk) { return; }
	for (int32 k = 0; k < (bCanyonSand ? 4 : 16); k++)
	{
		AActor* A = W->SpawnActor<AActor>(Wd(FVector(0.0, 0.0, -6000.0)), FRotator::ZeroRotator);
		Stirrers.Add(A);
		if (!A) { continue; }
		USphereComponent* Sp = NewObject<USphereComponent>(A); Sp->InitSphereRadius(k >= 12 ? 40.f : (k % 6) == 5 ? 34.f : 20.f); Sp->SetMobility(EComponentMobility::Movable);
		Sp->SetCollisionObjectType(ECC_Pawn); Sp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Sp->SetCollisionResponseToAllChannels(ECR_Overlap); Sp->SetGenerateOverlapEvents(true);
		A->SetRootComponent(Sp); Sp->RegisterComponent();
	}
}

/** The mist needs fog that light can be seen in: whichever height fog is lit here is made volumetric. */
void AGkMatch::Mist()
{
	UWorld* W = GetWorld(); if (!W) { return; } int32 Live = 0;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		TInlineComponentArray<UExponentialHeightFogComponent*> Cs(*It);
		for (UExponentialHeightFogComponent* C : Cs) { if (C->IsVisible()) { Live++; if (!C->bEnableVolumetricFog) { C->SetVolumetricFog(true); C->SetVolumetricFogDistance(9000.f); UE_LOG(LogTemp, Display, TEXT("GkMist: volumetric fog on for %s"), *It->GetName()); } } }
	}
	if (Live == 0 && Kind != 0 && Fog && Fog->GetComponent()) { UExponentialHeightFogComponent* F = Fog->GetComponent(); F->SetFogDensity(0.004f); F->SetVisibility(true); }
}

/** Gives a fighter the body chosen for him: one of the two mannequins for now, player one in red and the other in blue. */
void AGkMatch::DressFighter(int32 i)
{
	UPoseableMeshComponent* C = Body[i]; if (!C) { return; }
	InkMid[i] = nullptr; for (int32 k = 0; k < 4; k++) { BodyMid[i][k] = nullptr; }
	// Noa, the MeshBites character, as she is shipped: her own mesh, toon materials and outline, taken from her lit Blueprint
	if (Setup.Char[i] % 4 == 0)
	{
		// Kagu, brought over from the first project: his mesh and painted flat-colour map under the same toon shader. Player
		// two's Kagu wears the white colourway (Shiro).
		USkeletalMesh* Kagu = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Kagu/Kagu_V3.Kagu_V3"));
		UMaterialInterface* Skin = LoadObject<UMaterialInterface>(nullptr, (i == 1 && Setup.Char[0] % 4 == 0) ? TEXT("/Game/Kagu/MI_Shiro.MI_Shiro") : TEXT("/Game/Kagu/MI_Kagu.MI_Kagu"));
		UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Kagu/MI_Kagu_Outline.MI_Kagu_Outline"));
		if (Kagu && Skin)
		{
			C->EmptyOverrideMaterials(); C->SetSkinnedAssetAndUpdate(Kagu); for (int32 k = 0; k < C->GetNumMaterials(); k++) { C->SetMaterial(k, Skin); } C->SetOverlayMaterial(Ink);
			Rig[i].Init(C);
			if (USkeletalMeshComponent* A = Anim[i])
			{
				A->EmptyOverrideMaterials(); A->SetSkeletalMeshAsset(Kagu); for (int32 k = 0; k < A->GetNumMaterials(); k++) { A->SetMaterial(k, Skin); } A->SetOverlayMaterial(Ink);
				A->SetAnimationMode(EAnimationMode::AnimationSingleNode); Playing[i] = nullptr;
			}
			ClipSet[i] = (Clips[0].Num() > 0 && !FParse::Param(FCommandLine::Get(), TEXT("gknoclips"))) ? 0 : -1;
			return;
		}
	}
	if (Setup.Char[i] % 4 == 1)
	{
		bool bDone = false; UWorld* W = GetWorld();
		UClass* Bp = LoadObject<UClass>(nullptr, TEXT("/Game/MeshBites_Stylized_Anime_Shader/Art/Models/Characters/Noa_00/BP/BP_Noa_00_Lit.BP_Noa_00_Lit_C"));
		USkeletalMesh* Noa = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/MeshBites_Stylized_Anime_Shader/Art/Models/Characters/Noa_00/Meshes/SKM_Noa_00.SKM_Noa_00"));
		if (Noa)
		{
			C->SetOverlayMaterial(nullptr); C->EmptyOverrideMaterials(); C->SetSkinnedAssetAndUpdate(Noa); bDone = true;
			if (AActor* Shown = (W && Bp) ? W->SpawnActor<AActor>(Bp, FTransform(FVector(0.0, 0.0, -50000.0))) : nullptr)
			{
				TInlineComponentArray<UMeshComponent*> Ms(Shown); UMeshComponent* From = nullptr;
				for (UMeshComponent* M : Ms) { if (!From || M->GetNumMaterials() > From->GetNumMaterials()) { From = M; } }
				if (From)
				{
					// her Blueprint may show her as a still mesh; its materials go onto the skinned one slot by slot, by name where the names agree
					const TArray<FName> Mine = C->GetMaterialSlotNames(), Theirs = From->GetMaterialSlotNames();
					for (int32 k = 0; k < Mine.Num(); k++) { const int32 At2 = Theirs.Find(Mine[k]); const int32 Use = At2 != INDEX_NONE ? At2 : (k < From->GetNumMaterials() ? k : INDEX_NONE); if (Use != INDEX_NONE) { if (UMaterialInterface* Mat = From->GetMaterial(Use)) { C->SetMaterial(k, Mat); } } }
					if (UMaterialInterface* Over = From->GetOverlayMaterial()) { C->SetOverlayMaterial(Over); }
					UE_LOG(LogTemp, Display, TEXT("GkNoa: %d slots dressed from %s (%s, %d materials)"), Mine.Num(), *From->GetName(), *From->GetClass()->GetName(), From->GetNumMaterials());
				}
				Shown->Destroy();
			}
		}
		if (bDone)
		{
			Rig[i].Init(C);
			if (USkeletalMeshComponent* A = Anim[i])
			{
				A->SetOverlayMaterial(C->GetOverlayMaterial()); A->EmptyOverrideMaterials(); A->SetSkeletalMeshAsset(Noa);
				for (int32 k = 0; k < C->GetNumMaterials(); k++) { A->SetMaterial(k, C->GetMaterial(k)); }
				A->SetAnimationMode(EAnimationMode::AnimationSingleNode); Playing[i] = nullptr;
			}
			ClipSet[i] = (Clips[1].Num() > 0 && !FParse::Param(FCommandLine::Get(), TEXT("gknoclips"))) ? 1 : -1;
			return;
		}
	}
	ClipSet[i] = -1; if (Anim[i]) { Anim[i]->SetVisibility(false); }
	C->EmptyOverrideMaterials();
	static const TCHAR* Meshes[2] = { TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple") };
	{
		// The fighters are Unreal's mannequins, red and blue. The surface is MeshBites' Stylized Anime
		// Shader (cel shading read from the sun's direction) with its outline shell laid over the mesh.
		// (-gkfighter on the command line tries the stand-in human model instead; it is off by default.)
		USkeletalMesh* Model = FParse::Param(FCommandLine::Get(), TEXT("gkfighter")) ? LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fighter/SK_Fighter.SK_Fighter")) : nullptr;
		UMaterialInterface* Skin = Model ? LoadObject<UMaterialInterface>(nullptr, i ? TEXT("/Game/Fighter/MI_Fighter_P2.MI_Fighter_P2") : TEXT("/Game/Fighter/MI_Fighter_P1.MI_Fighter_P1")) : nullptr;
		if (Model && Skin)
		{
			C->SetSkinnedAssetAndUpdate(Model);
			for (int32 k = 0; k < C->GetNumMaterials(); k++) { C->SetMaterial(k, Skin); }
			if (UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fighter/MI_Fighter_Outline.MI_Fighter_Outline"))) { C->SetOverlayMaterial(Ink); }
		}
		else
		{
			if (USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, Meshes[Setup.Char[i] % 4 == 3 ? 1 : 0])) { C->SetSkinnedAssetAndUpdate(M); }
			const TCHAR* Side = i ? TEXT("P2") : TEXT("P1"); bool bAnime = true;
			for (int32 k = 0; k < C->GetNumMaterials(); k++)
			{
				const FString Path = FString::Printf(TEXT("/Game/GkMat/MI_Gk_%s_%s.MI_Gk_%s_%s"), Side, k == 0 ? TEXT("A") : TEXT("B"), Side, k == 0 ? TEXT("A") : TEXT("B"));
				if (UMaterialInterface* Cel = LoadObject<UMaterialInterface>(nullptr, *Path))
				{
					UMaterialInstanceDynamic* Dy = UMaterialInstanceDynamic::Create(ForestFighterCel ? ForestFighterCel.Get() : Cel, this); C->SetMaterial(k, Dy ? static_cast<UMaterialInterface*>(Dy) : Cel);
					if (Dy && k < 4)
					{
						Mids.Add(Dy); BodyMid[i][k] = Dy; BaseCol[i][k] = FLinearColor(0.8f, 0.8f, 0.8f); ShadeCol[i][k] = FLinearColor(0.4f, 0.4f, 0.4f);
						Cel->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Base  color")), BaseCol[i][k]); Cel->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Shadow_Base")), ShadeCol[i][k]);
					}
				}
				else
				{
					bAnime = false; UE_LOG(LogTemp, Warning, TEXT("Gekitotsu: the anime material %s did not load"), *Path);
					if (UMaterialInstanceDynamic* Tint = C->CreateDynamicMaterialInstance(k)) { Tint->SetVectorParameterValue(TEXT("Paint Tint"), i ? FLinearColor(0.05f, 0.22f, 0.95f) : FLinearColor(0.9f, 0.07f, 0.04f)); }
				}
			}
			if (bAnime) { if (UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GkMat/MI_Gk_Outline.MI_Gk_Outline"))) { InkMid[i] = UMaterialInstanceDynamic::Create(Ink, this); if (InkMid[i]) { Mids.Add(InkMid[i]); C->SetOverlayMaterial(InkMid[i]); } else { C->SetOverlayMaterial(Ink); } } }
			if (USkeletalMeshComponent* A = Anim[i])
			{
				if (USkeletalMesh* SM = Cast<USkeletalMesh>(C->GetSkinnedAsset()))
				{
					A->EmptyOverrideMaterials(); A->SetSkeletalMeshAsset(SM); for (int32 k = 0; k < C->GetNumMaterials(); k++) { A->SetMaterial(k, C->GetMaterial(k)); } A->SetOverlayMaterial(C->GetOverlayMaterial());
					A->SetAnimationMode(EAnimationMode::AnimationSingleNode); Playing[i] = nullptr;
					ClipSet[i] = (Clips[2].Num() > 0 && !FParse::Param(FCommandLine::Get(), TEXT("gknoclips"))) ? 2 : -1;
				}
			}
		}
	}
	Rig[i].Init(C);
}

// ------------------------------------------------------------------------------------------------ the animation lab
FString AGkMatch::PicksFile(bool bProject) const
{
	// the game's own copy, and the project's (so a choice made in a built game is still there after the next build)
	return bProject ? FString(TEXT("/Users/Shared/Epic Games/GekitotsuNative/Content/Gk/picks.txt")) : FPaths::ProjectContentDir() / TEXT("Gk/picks.txt");
}

void AGkMatch::LoadPicks()
{
	FString Text; if (!FFileHelper::LoadFileToString(Text, *PicksFile(true)) && !FFileHelper::LoadFileToString(Text, *PicksFile(false))) { return; }
	TArray<FString> Lines; Text.ParseIntoArrayLines(Lines); int32 N = 0;
	for (const FString& Ln : Lines)
	{
		TArray<FString> T; Ln.ParseIntoArray(T, TEXT("|")); if (T.Num() < 6 || Ln.StartsWith(TEXT("#"))) { continue; }
		const int32 Set = FCString::Atoi(*T[0]); if (Set < 0 || Set > 2) { continue; }
		FGkClip C; C.From = FCString::Atof(*T[3]); C.To = FCString::Atof(*T[4]); C.bLoop = FCString::Atoi(*T[5]) != 0;
		const FString Path = T[2].TrimStartAndEnd();
		if (Path != TEXT("none")) { C.Anim = LoadObject<UAnimSequence>(nullptr, *Path); if (!C.Anim) { continue; } C.Anim->bForceRootLock = true; ClipKeep.Add(C.Anim); }
		Clips[Set].Add(FName(*T[1].TrimStartAndEnd()), C); N++; Picked.Add(FString::Printf(TEXT("%d|%s"), Set, *T[1].TrimStartAndEnd()));
	}
	UE_LOG(LogTemp, Display, TEXT("GkClip: %d chosen in the lab"), N);
}

void AGkMatch::LabSave()
{
	FString Out = TEXT("# set (0 Kagu, 1 Noa, 2 the robot) | move or state | clip, or none for the hand pose | from | to | loop\n");
	for (int32 Set = 0; Set < 3; Set++) { for (const auto& It : Clips[Set])
	{
		if (!Picked.Contains(FString::Printf(TEXT("%d|%s"), Set, *It.Key.ToString()))) { continue; }   // only what was chosen by hand: the rest follows the game's own table
		Out += FString::Printf(TEXT("%d|%s|%s|%.3f|%.3f|%d\n"), Set, *It.Key.ToString(), It.Value.Anim ? *It.Value.Anim->GetPathName() : TEXT("none"), It.Value.From, It.Value.To, It.Value.bLoop ? 1 : 0);
	} }
	const bool bA = FFileHelper::SaveStringToFile(Out, *PicksFile(false)), bB = FFileHelper::SaveStringToFile(Out, *PicksFile(true));
	LabNote = (bA || bB) ? TEXT("SAVED") : TEXT("COULD NOT SAVE"); LabNoteTimer = 90;
}

namespace
{
	/** What kind of clip this is, from its name: 0 movement, 1 block, 2 hit, 3 fall and get up, 4 punch, 5 kick, 6 combo, 7 energy. */
	int32 GkClipCat(const FString& N)
	{
		auto Has = [&N](const TCHAR* W) { return N.Contains(W); };
		if (Has(TEXT("Anim_SC_"))) { return 8; }
		if (Has(TEXT("SpecialMove")) || Has(TEXT("Shoryuken"))) { return 7; }
		if (Has(TEXT("Combo"))) { return 6; }
		if (Has(TEXT("Block")) || Has(TEXT("Defense"))) { return 1; }
		if (Has(TEXT("Fall")) || Has(TEXT("Dead")) || Has(TEXT("Kockout")) || Has(TEXT("StandUp")) || Has(TEXT("Rise"))) { return 3; }
		if (Has(TEXT("Hit")) || Has(TEXT("React")) || Has(TEXT("Injured"))) { return 2; }
		if (Has(TEXT("Kick"))) { return 5; }
		if (Has(TEXT("Punch")) || Has(TEXT("Jab")) || Has(TEXT("Hook")) || Has(TEXT("ppercut")) || Has(TEXT("Uperrcut")) || Has(TEXT("Fist")) || Has(TEXT("Elbow")) || Has(TEXT("OverH"))) { return 4; }
		return 0;
	}
	/** Which page of the grid a move belongs on: 3 punches, 4 kicks, 5 in the air, 6 specials, 7 supers, 8 throws. */
	int32 GkMoveCat(const FString& K)
	{
		if (K == TEXT("SA1") || K == TEXT("SA2") || K == TEXT("SA3") || K == TEXT("CA")) { return 7; }
		if (K == TEXT("throw")) { return 8; }
		if (K.StartsWith(TEXT("j")) || K.StartsWith(TEXT("nj")) || K == TEXT("AUP") || K == TEXT("ABK") || K == TEXT("ASPK") || K.StartsWith(TEXT("ATAT")) || K.StartsWith(TEXT("AHOME")) || K.StartsWith(TEXT("HOME"))) { return 5; }
		if (K == TEXT("TFK2") || K == TEXT("TFK3")) { return 4; }
		if (K == TEXT("CB2")) { return 3; }
		if (K.Len() == 3 && (K[0] == '5' || K[0] == '2')) { return K.EndsWith(TEXT("K")) ? 4 : 3; }
		return 6;
	}
}

/** Opens the lab on player one's character: a grid of every state and move by kind, and a drop-down of clips for each. */
void AGkMatch::LabOpen()
{
	Screen = 4; bMenu = false; bLabDrop = false; LabFrame = 0; LabClock = 0.f; LabTryAt = -2;
	if (ClipSet[0] < 0) { Setup.Char[0] = 2; DressFighter(0); }
	LabSet = FMath::Clamp(ClipSet[0], 0, 2);
	struct FState { const TCHAR* Slot; const TCHAR* Label; int32 Cat; };
	static const FState States[] = {
		{ TEXT("idle"), TEXT("Idle stance"), 0 }, { TEXT("walkf"), TEXT("Walk forward"), 0 }, { TEXT("walkb"), TEXT("Walk back"), 0 }, { TEXT("crouch"), TEXT("Crouch"), 0 }, { TEXT("prejump"), TEXT("Jump start"), 0 }, { TEXT("jump"), TEXT("Jump"), 0 },
		{ TEXT("land"), TEXT("Landing"), 0 }, { TEXT("dashf"), TEXT("Dash forward"), 0 }, { TEXT("dashb"), TEXT("Dash back"), 0 }, { TEXT("run"), TEXT("Run"), 0 }, { TEXT("drush"), TEXT("Drive rush"), 0 }, { TEXT("tech"), TEXT("Recovery flip"), 0 }, { TEXT("win"), TEXT("Win pose"), 0 },
		{ TEXT("blockS"), TEXT("Standing block"), 1 }, { TEXT("blockC"), TEXT("Crouching block"), 1 }, { TEXT("parry"), TEXT("Parry"), 1 }, { TEXT("prec"), TEXT("Perfect parry"), 1 },
		{ TEXT("hitL"), TEXT("Hit: light"), 2 }, { TEXT("hitM"), TEXT("Hit: medium"), 2 }, { TEXT("hitH"), TEXT("Hit: heavy"), 2 }, { TEXT("hitLow"), TEXT("Hit: legs"), 2 }, { TEXT("hitCr"), TEXT("Hit: crouching"), 2 }, { TEXT("crumple"), TEXT("Crumple"), 2 },
		{ TEXT("splat"), TEXT("Wall splat"), 2 }, { TEXT("ahit"), TEXT("Hit in the air"), 2 }, { TEXT("kd"), TEXT("Knocked down"), 2 }, { TEXT("getup"), TEXT("Get up"), 2 }, { TEXT("thrown"), TEXT("Being thrown"), 2 },
		{ TEXT("grab"), TEXT("Grab"), 8 }, { TEXT("throw1"), TEXT("Throw forward"), 8 }, { TEXT("throw2"), TEXT("Throw back"), 8 }, { TEXT("throw3"), TEXT("Throw up"), 8 }, { TEXT("throw4"), TEXT("Throw down (slam)"), 8 } };
	LabSlots.Reset();
	for (const FState& St : States) { FLabSlot L; L.Slot = FName(St.Slot); L.Label = St.Label; L.Cat = St.Cat; LabSlots.Add(L); }
	for (int32 k = 0; k < MoveClip.Num(); k++) { const FString Key = UTF8_TO_TCHAR(gk::moveKey(k)); FLabSlot L; L.Slot = MoveClip[k]; L.Label = FString::Printf(TEXT("%s   %s"), UTF8_TO_TCHAR(gk::moveName(k)), *Key); L.Cat = GkMoveCat(Key); LabSlots.Add(L); }
	LabList.Reset();
	IAssetRegistry& Reg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	static const TCHAR* Suffixes[3] = { TEXT("_Kagu"), TEXT("_Noa"), TEXT("_Manny") }; const FString Suffix = Suffixes[LabSet];
	for (const TCHAR* Root : { TEXT("/Game/UnarmFightAnimBundle"), TEXT("/Game/Superhero_Combat"), TEXT("/Game/SpecialMoves_Energy"), TEXT("/Game/FightAnimations") })
	{
		TArray<FAssetData> Found; Reg.GetAssetsByPath(FName(Root), Found, true);
		for (const FAssetData& A : Found)
		{
			const FString Name = A.AssetName.ToString(), Path = A.GetObjectPathString();
			if (!Name.EndsWith(Suffix) || A.AssetClassPath.GetAssetName() != TEXT("AnimSequence")) { continue; }
			if (Path.Contains(TEXT("SpecialMoves_Energy")) && (!Path.Contains(TEXT("/InPlace/Male/")) || Path.Contains(TEXT("/Reverse/")))) { continue; }
			FString Short = Name.LeftChop(Suffix.Len()); int32 V = Short.Find(TEXT("V"));
			if (Short.StartsWith(TEXT("A_Unarmed")) && V != INDEX_NONE && V + 3 < Short.Len()) { Short = Short.Mid(V + 3); }
			Short.RemoveFromStart(TEXT("Anim_SC_")); Short.RemoveFromStart(TEXT("A_")); Short.RemoveFromStart(TEXT("SpecialMove_")); Short.RemoveFromEnd(TEXT("_IP")); Short.ReplaceInline(TEXT("_"), TEXT(" "));
			const TCHAR* Pack = Path.Contains(TEXT("FightAnimations")) ? TEXT("FIGHT") : Path.Contains(TEXT("SpecialMoves")) ? TEXT("ENERGY") : Path.Contains(TEXT("Style_One")) ? TEXT("HERO 1") : Path.Contains(TEXT("Style_Two")) ? TEXT("HERO 2")
				: Path.Contains(TEXT("V1")) ? TEXT("UNARMED 1") : Path.Contains(TEXT("V2")) ? TEXT("UNARMED 2") : Path.Contains(TEXT("V3")) ? TEXT("UNARMED 3") : TEXT("UNARMED 4");
			FLabClip L; L.Path = Path; L.Label = FString::Printf(TEXT("%s      %s"), *Short, Pack); L.Cat = GkClipCat(Name); LabList.Add(L);
		}
	}
	LabList.Sort([](const FLabClip& A, const FLabClip& B) { return A.Label < B.Label; });
	LabSlot = FMath::Clamp(LabSlot, 0, FMath::Max(0, LabSlots.Num() - 1)); if (LabSlots.IsValidIndex(LabSlot)) { LabCat = LabSlots[LabSlot].Cat; }
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->bShowMouseCursor = true; FInputModeGameAndUI In; In.SetHideCursorDuringCapture(false); In.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(In);
	}
}

void AGkMatch::LabClose()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) { PC->bShowMouseCursor = false; PC->SetInputMode(FInputModeGameOnly()); }
	bLabDrop = false; LabTryAt = -2; Screen = 0; MenuAt = 6; Restart();
}

/** Puts a clip (or none: the hand pose) on the chosen move, and saves. */
void AGkMatch::LabPick(int32 ClipIndex)
{
	if (!LabSlots.IsValidIndex(LabSlot)) { return; }
	const FName Slot = LabSlots[LabSlot].Slot; const FString Sn = Slot.ToString();
	FGkClip C; C.From = 0.f; C.To = 1.f; C.bLoop = Sn == TEXT("idle") || Sn == TEXT("walkf") || Sn == TEXT("walkb") || Sn == TEXT("run") || Sn == TEXT("drush") || Sn == TEXT("blockS") || Sn == TEXT("blockC");
	C.Anim = LabList.IsValidIndex(ClipIndex) ? LoadObject<UAnimSequence>(nullptr, *LabList[ClipIndex].Path) : nullptr;
	if (C.Anim) { C.Anim->bForceRootLock = true; ClipKeep.Add(C.Anim); }
	Picked.Add(FString::Printf(TEXT("%d|%s"), LabSet, *Sn)); Clips[LabSet].Add(Slot, C); bLabDrop = false; LabTryAt = -2; LabFrame = 0; LabClock = 0.f; LabSave();
}

/** A click in the lab, on whatever the pointer was over when the screen was last drawn. */
void AGkMatch::LabClick(int32 Id)
{
	if (Id < 0) { return; }
	if (Id == 0) { bLabDrop = false; return; }
	if (Id == 1) { LabClose(); return; }
	if (Id == 7) { static const int32 Next[4] = { 1, 2, 0, 0 }; Setup.Char[0] = Next[Setup.Char[0] % 4]; DressFighter(0); LabOpen(); return; }
	if (Id >= 2 && Id <= 6)
	{
		if (FGkClip* C = LabSlots.IsValidIndex(LabSlot) ? Clips[LabSet].Find(LabSlots[LabSlot].Slot) : nullptr)
		{
			if (Id == 2) { C->From = FMath::Clamp(C->From - 0.05f, 0.f, C->To - 0.05f); } if (Id == 3) { C->From = FMath::Clamp(C->From + 0.05f, 0.f, C->To - 0.05f); }
			if (Id == 4) { C->To = FMath::Clamp(C->To - 0.05f, C->From + 0.05f, 1.f); } if (Id == 5) { C->To = FMath::Clamp(C->To + 0.05f, C->From + 0.05f, 1.f); }
			if (Id == 6) { C->bLoop = !C->bLoop; }
			Picked.Add(FString::Printf(TEXT("%d|%s"), LabSet, *LabSlots[LabSlot].Slot.ToString()));
			LabFrame = 0; LabClock = 0.f; LabSave();
		}
		return;
	}
	if (Id >= 100 && Id < 110) { LabCat = Id - 100; bLabDrop = false; for (int32 k = 0; k < LabSlots.Num(); k++) { if (LabSlots[k].Cat == LabCat) { LabSlot = k; break; } } LabFrame = 0; return; }
	if (Id >= 200 && Id < 210) { LabDropCat = Id - 200; LabScroll = 0; return; }
	if (Id >= 1000 && Id < 9999)
	{
		const int32 To = Id - 1000; if (!LabSlots.IsValidIndex(To)) { return; }
		if (bLabDrop && To == LabSlot) { bLabDrop = false; return; }
		LabSlot = To; LabCat = LabSlots[To].Cat; bLabDrop = true; LabScroll = 0; LabFrame = 0; LabClock = 0.f;
		static const int32 Want[9] = { 0, 1, 2, 4, 5, 8, 7, 7, 6 }; LabDropCat = Want[FMath::Clamp(LabSlots[To].Cat, 0, 8)];
		const FString Sn = LabSlots[To].Slot.ToString(); if (Sn == TEXT("kd") || Sn == TEXT("getup") || Sn == TEXT("crumple")) { LabDropCat = 3; }
		if (const FGkClip* C = Clips[LabSet].Find(LabSlots[To].Slot)) { if (C->Anim) { LabDropCat = GkClipCat(C->Anim->GetName()); } }
		return;
	}
	if (Id == 9999) { LabPick(-1); return; }
	if (Id >= 10000) { LabPick(Id - 10000); }
}

// ------------------------------------------------------------------------------------------------ the sky and weather
namespace
{
	/** Calls a Blueprint function by name, filling its inputs by kind: the first object input, numbers, and whole numbers. */
	bool GkCall(UObject* On, const TCHAR* Name, UObject* Obj, double Num, int32 Whole = 0, double Num2 = 0.0)
	{
		UFunction* F = On ? On->FindFunction(FName(Name)) : nullptr; if (!F) { return false; }
		uint8* Buf = static_cast<uint8*>(FMemory_Alloca(FMath::Max<int32>(F->ParmsSize, 1))); FMemory::Memzero(Buf, F->ParmsSize);
		int32 Numbers = 0;
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Buf);
			if (It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm)) { continue; }
			if (FObjectPropertyBase* O = CastField<FObjectPropertyBase>(*It)) { O->SetObjectPropertyValue_InContainer(Buf, Obj); }
			else if (FDoubleProperty* D = CastField<FDoubleProperty>(*It)) { D->SetPropertyValue_InContainer(Buf, Numbers++ == 0 ? Num : Num2); }
			else if (FFloatProperty* Fl = CastField<FFloatProperty>(*It)) { Fl->SetPropertyValue_InContainer(Buf, float(Numbers++ == 0 ? Num : Num2)); }
			else if (FIntProperty* I = CastField<FIntProperty>(*It)) { I->SetPropertyValue_InContainer(Buf, Whole); }
		}
		On->ProcessEvent(F, Buf);
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It) { It->DestroyValue_InContainer(Buf); }
		return true;
	}
	void GkSetBool(UObject* On, const TCHAR* Name, bool bValue) { if (FBoolProperty* P = FindFProperty<FBoolProperty>(On->GetClass(), FName(Name))) { P->SetPropertyValue_InContainer(On, bValue); } else { UE_LOG(LogTemp, Warning, TEXT("GkSky: no switch named %s"), Name); } }
}

void AGkMatch::MakeSky()
{
	UWorld* W = GetWorld(); if (!W) { return; }
	if (Kind == 2 && (bOwnLight || FParse::Param(FCommandLine::Get(), TEXT("gknosky")))) { return; }
	const FGkStage* St = Stages.IsValidIndex(Setup.Stage) ? &Stages[Setup.Stage] : nullptr;
	FString Preset = St && !St->Sky.IsEmpty() ? St->Sky : FString(TEXT("Desert")); FParse::Value(FCommandLine::Get(), TEXT("-gksky="), Preset); if (!GkSkyForce.IsEmpty()) { Preset = GkSkyForce; }
	const FString Path = FString::Printf(TEXT("/Game/SoStylized/Environment/Sky/PRESETS/BP_StylizedSky_%s.BP_StylizedSky_%s_C"), *Preset, *Preset);
	UClass* Cls = LoadObject<UClass>(nullptr, *Path);
	if (!Cls) { UE_LOG(LogTemp, Warning, TEXT("GkSky: %s did not load; the plain sky stays"), *Path); return; }
	static const TCHAR* Names[4] = { TEXT("DA_Weather_Clear"), TEXT("DA_Weather_Overcast"), TEXT("DA_Weather_Thunderstorm_Distant"), TEXT("DA_Weather_Thunderstorm") };
	for (const TCHAR* N : Names) { Weathers.Add(LoadObject<UObject>(nullptr, *FString::Printf(TEXT("/Game/SoStylized/Environment/Sky/Weather/%s.%s"), N, N))); }
	if (FParse::Param(FCommandLine::Get(), TEXT("gkskylog")))
	{
		for (TFieldIterator<UFunction> It(Cls, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			if (It->GetOuter() && It->GetOuter()->GetName().StartsWith(TEXT("BP_StylizedSky")))
			{
				FString Sig; for (TFieldIterator<FProperty> P(*It); P && P->HasAnyPropertyFlags(CPF_Parm); ++P) { Sig += FString::Printf(TEXT(" %s:%s"), *P->GetName(), *P->GetClass()->GetName()); }
				UE_LOG(LogTemp, Display, TEXT("GkSky fn %s (%s )"), *It->GetName(), *Sig);
			}
		}
		for (TFieldIterator<FProperty> It(Cls, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			if (It->GetOwnerClass() && It->GetOwnerClass()->GetName().StartsWith(TEXT("BP_StylizedSky")))
			{
				FString Val; It->ExportText_InContainer(0, Val, Cls->GetDefaultObject(), nullptr, nullptr, PPF_None); UE_LOG(LogTemp, Display, TEXT("GkSky var %s:%s = %s"), *It->GetName(), *It->GetClass()->GetName(), *Val.Left(120));
			}
		}
	}
	const FTransform At(FRotator::ZeroRotator, Anchor.GetLocation());
	AActor* Sky = W->SpawnActorDeferred<AActor>(Cls, At, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Sky) { return; }
	// the fight drives the weather: no clock, no weather of its own choosing
	GkSetBool(Sky, TEXT("DayCycleEnabled?"), false);
	if (FArrayProperty* Cycle = FindFProperty<FArrayProperty>(Cls, TEXT("DynamicWeatherCycle"))) { FScriptArrayHelper H(Cycle, Cycle->ContainerPtrToValuePtr<void>(Sky)); H.EmptyValues(); }
	Sky->FinishSpawning(At);
	SkyActor = Sky;
	// the sky's clock runs in seconds: a 600 second day, then a 480 second night. The fight is held at dusk.
	double Time = St ? St->Time : 500.0; FParse::Value(FCommandLine::Get(), TEXT("-gktime="), Time); if (GkTimeForce >= 0.0) { Time = GkTimeForce; }
	if (!GkCall(Sky, TEXT("Set New Time"), nullptr, Time)) { UE_LOG(LogTemp, Warning, TEXT("GkSky: no Set New Time")); }
	SetWeather(0, 0.5);
}

void AGkMatch::SetWeather(int32 Index, double BlendSeconds)
{
	if (!SkyActor || !Weathers.IsValidIndex(Index) || !Weathers[Index] || Index == WeatherAt) { return; }
	WeatherAt = Index;
	if (!GkCall(SkyActor, TEXT("Set New Static Weather"), Weathers[Index], BlendSeconds)) { UE_LOG(LogTemp, Warning, TEXT("GkSky: no Set New Static Weather")); }
}

// ------------------------------------------------------------------------------------------------ a pack's map as a stage
/**
 * Sets the fight down in a dressed map. A stage with fixed numbers stands where they say; otherwise the ground near the
 * map's player start is searched for the most level 16 metre line with a clear view from the camera's side. The fight's
 * floor is the highest ground under that line; if the ground under it is uneven a plain slab is laid to stand on.
 */
bool AGkMatch::TryPlace()
{
	UWorld* W = GetWorld(); if (!W) { return false; }
	const FGkStage* St = Stages.IsValidIndex(Setup.Stage) ? &Stages[Setup.Stage] : nullptr;
	FCollisionQueryParams Q(FName(TEXT("GkPlace")), true, this);
	auto Ground = [&](const FVector& P, double& Z) { FHitResult H; if (W->LineTraceSingleByChannel(H, P + FVector(0, 0, 3000), P - FVector(0, 0, 8000), ECC_WorldStatic, Q)) { Z = H.ImpactPoint.Z; return true; } return false; };
	FVector Base = FVector::ZeroVector; double BaseYaw = 0.0; bool bFixed = St && St->bFixed;
	if (bFixed) { Base = St->At; BaseYaw = St->Yaw; }
	else { TActorIterator<APlayerStart> It(W); if (It) { Base = It->GetActorLocation(); BaseYaw = It->GetActorRotation().Yaw; } }
	{ FString Txt; if (FParse::Value(FCommandLine::Get(), TEXT("-gkanchor="), Txt)) { TArray<FString> N; Txt.ParseIntoArray(N, TEXT(",")); if (N.Num() >= 4) { Base = FVector(FCString::Atod(*N[0]), FCString::Atod(*N[1]), FCString::Atod(*N[2])); BaseYaw = FCString::Atod(*N[3]); bFixed = true; } } }
	double Z0 = 0.0; if (!Ground(Base, Z0) && PlaceWait < 6.f) { return false; }   // the ground has not streamed in yet
	auto Score = [&](const FVector& C, double Yaw, double& Top, double& Range)
	{
		const FQuat R = FRotator(0.0, Yaw, 0.0).Quaternion(); double Lo = 1e9, Hi = -1e9;
		for (int32 k = -4; k <= 4; k++) { double Z; if (!Ground(C + R.RotateVector(FVector(0.0, k * 200.0, 0.0)), Z)) { return false; } Lo = FMath::Min(Lo, Z); Hi = FMath::Max(Hi, Z); }
		for (int32 k = -1; k <= 1; k++) { double Z; if (Ground(C + R.RotateVector(FVector(-250.0, k * 600.0, 0.0)), Z)) { Hi = FMath::Max(Hi, Z - 15.0); } }
		Top = Hi; Range = Hi - Lo;
		// nothing between the camera's side and the fighters, and nothing standing on the line itself
		const FVector Mid(C.X, C.Y, Hi + 110.0); FHitResult H;
		for (int32 k = -1; k <= 1; k++) { if (W->LineTraceSingleByChannel(H, Mid + R.RotateVector(FVector(-1300.0, k * 700.0, 120.0)), Mid + R.RotateVector(FVector(0.0, k * 700.0, 0.0)), ECC_Visibility, Q)) { return false; } }
		if (W->LineTraceSingleByChannel(H, Mid + R.RotateVector(FVector(0.0, -800.0, 0.0)), Mid + R.RotateVector(FVector(0.0, 800.0, 0.0)), ECC_Visibility, Q)) { return false; }
		return true;
	};
	FVector Best = Base; double BestYaw = BaseYaw, BestTop = Z0, BestRange = 0.0, BestCost = 1e18; bool bFound = false;
	if (bFixed) { bFound = Score(Base, BaseYaw, BestTop, BestRange); if (!bFound) { BestTop = Z0; BestRange = 999.0; bFound = true; } }
	else
	{
		for (int32 gx = -6; gx <= 6; gx++) { for (int32 gy = -6; gy <= 6; gy++) { for (int32 a = 0; a < 12; a++)
		{
			const FVector C = Base + FVector(gx * 400.0, gy * 400.0, 0.0); const double Yaw = BaseYaw + a * 30.0; double Top, Range;
			if (!Score(C, Yaw, Top, Range)) { continue; }
			// how much there is to look at behind the fight: the further the view runs before it meets something, up to a point, the better
			FHitResult H; const FQuat R = FRotator(0.0, Yaw, 0.0).Quaternion(); double Open = 0.0;
			for (int32 k = -1; k <= 1; k++) { const FVector From(C.X, C.Y, Top + 200.0); Open += W->LineTraceSingleByChannel(H, From, From + R.RotateVector(FVector(9000.0, k * 2500.0, 600.0)), ECC_Visibility, Q) ? FMath::Min(double(H.Distance), 6000.0) : 4500.0; }
			const double Cost = Range * 4.0 + FVector::Dist2D(C, Base) * 0.02 - Open * 0.01;
			if (Cost < BestCost) { BestCost = Cost; Best = C; BestYaw = Yaw; BestTop = Top; BestRange = Range; bFound = true; }
		} } }
		if (!bFound) { BestTop = Z0; BestRange = 999.0; }
	}
	Anchor = FTransform(FRotator(0.0, BestYaw, 0.0), FVector(Best.X, Best.Y, BestTop + 1.0));
	SetActorTransform(Anchor);
	if (St) { TArray<FString> N; St->Get(TEXT("sun")).ParseIntoArray(N, TEXT(",")); if (N.Num() >= 3) { GkApplySun(W, Anchor.GetRotation().Rotator().Yaw + 180.0 + FCString::Atod(*N[0]), FCString::Atod(*N[1]), FCString::Atod(*N[2]) / 100.0, N.Num() >= 6 ? FLinearColor(FCString::Atof(*N[3]) / 100.f, FCString::Atof(*N[4]) / 100.f, FCString::Atof(*N[5]) / 100.f) : FLinearColor::White, N.Num() >= 6); } }
	if (FloorSlab) { FloorSlab->SetVisibility(BestRange > 12.0 && !(St && St->Get(TEXT("slab")) == TEXT("off"))); }
	if (SkyActor) { SkyActor->SetActorLocation(Anchor.GetLocation()); }
	if (St && St->Map.Contains(TEXT("/DreamscapeEastLands/")) && !ShrineWater)
	{
		UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		UMaterialInterface* Water = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DreamscapeSeries/DreamscapeEastLands/Materials/Water/MI_Eastlands_LakeSmall.MI_Eastlands_LakeSmall"));
		if (Plane && Water)
		{
			ShrineWater = NewObject<UStaticMeshComponent>(this);
			ShrineWater->SetMobility(EComponentMobility::Movable);
			ShrineWater->SetupAttachment(RootComponent);
			ShrineWater->SetStaticMesh(Plane);
			ShrineWater->SetMaterial(0, Water);
			ShrineWater->SetRelativeLocation(FVector(-35.0, 0.0, 3.0));
			ShrineWater->SetRelativeScale3D(FVector(3.2, (2.0 * gk::STAGE * Cm + 100.0) / 100.0, 1.0));
			ShrineWater->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ShrineWater->SetCastShadow(false);
			ShrineWater->RegisterComponent();
			UE_LOG(LogTemp, Display, TEXT("GkStage: shrine water surface placed"));
		}
	}
	PlaceNote = FString::Printf(TEXT("%.0f,%.0f,%.0f,%.0f  ground uneven by %.0f cm%s"), Best.X, Best.Y, BestTop + 1.0, FMath::Fmod(BestYaw + 720.0, 360.0), BestRange, bFound ? TEXT("") : TEXT("  (no clear level line found)"));
	UE_LOG(LogTemp, Display, TEXT("GkStage placed at %s"), *PlaceNote);
	return true;
}

/** On a pack's map the sky and weather system owns the sun, the sky light, the atmosphere and the fog: the map's own are put out. */
void AGkMatch::QuietMapLights()
{
	UWorld* W = GetWorld(); if (!W || !SkyActor) { return; }
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It; if (A == this || A == SkyActor || A == Fog || RoomLights.Contains(A)) { continue; }
		const FString Cls = A->GetClass()->GetName();
		if (Cls.Contains(TEXT("SkySphere")) || Cls.Contains(TEXT("Sky_Sphere")) || Cls.Contains(TEXT("SkyDome")) || Cls.Contains(TEXT("UltraDynamicSky"))) { A->SetActorHiddenInGame(true); }
		TInlineComponentArray<USceneComponent*> Cs(A);
		for (USceneComponent* C : Cs)
		{
			if (C->IsA<UDirectionalLightComponent>() || C->IsA<USkyLightComponent>() || C->IsA<UExponentialHeightFogComponent>() || C->IsA<USkyAtmosphereComponent>() || C->IsA<UVolumetricCloudComponent>()) { if (C->IsVisible()) { C->SetVisibility(false); } }
		}
	}
}

/** Starts a fight on the chosen stage: here and now if this map holds it, otherwise by opening its map. */
void AGkMatch::StartStage(int32 Index)
{
	if (!Stages.IsValidIndex(Index)) { return; }
	Setup.Stage = Index; Setup.Mode = Mode; Setup.Level = Level; Setup.Dummy = Dummy;
	const FGkStage& St = Stages[Index];
	if (St.Map == MapName() && St.Kind != 2) { Kind = St.Kind; Screen = 3; bMenu = false; Restart(); CamD.bSnap = true; return; }
	if (St.Map == MapName() && Kind == 2) { Screen = 3; bMenu = false; Restart(); CamD.bSnap = true; return; }
	Setup.bGo = true;
	UGameplayStatics::OpenLevel(this, FName(*St.Map), true, TEXT("game=/Script/GekitotsuNative.GkGameMode"));
}

void AGkMatch::EndPlay(const EEndPlayReason::Type Reason)
{
	delete Pads; Pads = nullptr; if (Current == this) { Current = nullptr; }
	Super::EndPlay(Reason);
}

void AGkMatch::Restart()
{
	gk::newMatch(S, Mode == 1 && Screen == 3, 2); gk::aiMake(Cpu[0], Level); gk::aiMake(Cpu[1], Level);
	OverTimer = 0; CallHold = 0; Hist.Reset(); LastIn = -1; Adv = -999; bExchange = false; Sparks.Reset();
	for (int32 i = 0; i < 2; i++) { Combo[i] = 0; ComboTimer[i] = 0; NoteTimer[i] = 0; ActFrame[i] = 0; bWasIdle[i] = true; }
}
void AGkMatch::CamKick(float Roll, float Zm, float Fov, float Yaw)
{
	if (FMath::Abs(Roll) > FMath::Abs(CamD.KRoll)) { CamD.KRoll = Roll; } CamD.KZm = FMath::Min(CamD.KZm, Zm); CamD.KFov = FMath::Max(CamD.KFov, Fov); if (FMath::Abs(Yaw) > FMath::Abs(CamD.KYaw)) { CamD.KYaw = Yaw; }
}
void AGkMatch::Say(const TCHAR* Word, int32 Hold) { CallWord = Word; CallAge = 0; CallHold = Hold; }
void AGkMatch::Remark(int32 Who, const TCHAR* Text) { Note[Who & 1] = Text; NoteTimer[Who & 1] = 60; }

// ------------------------------------------------------------------------------------------------ controls
/**
 * The browser game's buttons: Light, Medium, Heavy, Special, Assist, Parry, Impact, Homing.
 * Keyboard as the browser has it: WASD, J K L, I special, Space or Shift assist, U parry, O impact, H homing (arrows and the number pad too).
 * A stick or pad is read straight from USB when the system shows one; otherwise through Unreal's own gamepad input.
 */
int32 AGkMatch::ReadPlayer() const
{
	const UWorld* W = GetWorld(); const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; if (!PC) { return 0; }
	auto K = [PC](const FKey& Key) { return PC->IsInputKeyDown(Key); };
	int32 In = PadDirs;
	const float SX = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX), SY = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	if (K(EKeys::A) || K(EKeys::Left) || K(EKeys::Gamepad_DPad_Left) || SX < -0.5f) { In |= gk::IN_L; }
	if (K(EKeys::D) || K(EKeys::Right) || K(EKeys::Gamepad_DPad_Right) || SX > 0.5f) { In |= gk::IN_R; }
	if (K(EKeys::W) || K(EKeys::Up) || K(EKeys::Gamepad_DPad_Up) || SY > 0.5f) { In |= gk::IN_U; }
	if (K(EKeys::S) || K(EKeys::Down) || K(EKeys::Gamepad_DPad_Down) || SY < -0.5f) { In |= gk::IN_D; }
	static const int32 Bits[8] = { gk::IN_LT, gk::IN_MD, gk::IN_HV, gk::IN_SP, gk::IN_AS, gk::IN_PA, gk::IN_DI, gk::IN_HM };
	static const FKey Keys[8] = { EKeys::J, EKeys::K, EKeys::L, EKeys::I, EKeys::SpaceBar, EKeys::U, EKeys::O, EKeys::H };
	static const FKey Keys2[8] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFive, EKeys::NumPadZero, EKeys::NumPadFour, EKeys::NumPadSix, EKeys::Add };
	for (int32 a = 0; a < 8; a++)
	{
		bool bOn = K(Keys[a]) || K(Keys2[a]);
		if (PadCount > 0) { bOn = bOn || (PadBind[a] >= 0 && PadBind[a] < 32 && ((PadNow >> PadBind[a]) & 1u) != 0); }
		else { bOn = bOn || (Bind[a].IsValid() && K(Bind[a])); }
		if (bOn) { In |= Bits[a]; }
	}
	if (K(EKeys::LeftShift)) { In |= gk::IN_AS; }
	return In;
}

void AGkMatch::SaveBinds() const
{
	FString Out; for (int32 a = 0; a < 8; a++) { Out += FString::Printf(TEXT("%s %d\n"), *Bind[a].GetFName().ToString(), PadBind[a]); }
	FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("gk_buttons.txt")));
}
void AGkMatch::LoadBinds()
{
	FString Text; if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("gk_buttons.txt")))) { return; }
	TArray<FString> Lines; Text.ParseIntoArrayLines(Lines);
	for (int32 a = 0; a < FMath::Min(8, Lines.Num()); a++)
	{
		TArray<FString> T; Lines[a].ParseIntoArrayWS(T); if (T.Num() < 2) { continue; }
		const FKey Key = FKey(FName(*T[0])); if (Key.IsValid()) { Bind[a] = Key; }
		PadBind[a] = FMath::Clamp(FCString::Atoi(*T[1]), -1, 31);
	}
}

/** What the training dummy does, as the browser game has it: stand, crouch, jump, block everything, or fight back. */
int32 AGkMatch::DummyInput()
{
	const gk::Fighter& D = S.p[1]; const gk::Fighter& A = S.p[0];
	switch (Dummy)
	{
		case 1: return gk::IN_D;
		case 2: return gk::IN_U;
		case 3:
		{
			const gk::Move* M = A.st == gk::ATK ? &gk::move(A.mv) : nullptr; const bool bLow = M && (M->g == 'l' || (M->nhits > 0 && M->hits[0].g == 'l'));
			return (D.x >= A.x ? gk::IN_R : gk::IN_L) | (bLow ? gk::IN_D : 0);
		}
		case 4: return gk::aiThink(Cpu[1], S, 1);
		default: return 0;
	}
}

/** The menu is drawn by the HUD; this reads it. TAB or START opens it, and the match waits while it is open. */
void AGkMatch::MenuInput()
{
	UWorld* W = GetWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; if (!PC) { return; }
	auto Hit = [PC](const FKey& Key) { return PC->WasInputKeyJustPressed(Key); };
	const uint32 Fresh = PadNow & ~PadPrev; const int32 FreshDir = PadDirs & ~PadDirPrev;
	auto Raw = [this, Fresh](int32 Action) { return PadBind[Action] >= 0 && PadBind[Action] < 32 && ((Fresh >> PadBind[Action]) & 1u) != 0; };
	const bool bStart = ((Fresh >> 9) & 1u) != 0, bSelect = ((Fresh >> 8) & 1u) != 0;
	const bool bToggle = Hit(EKeys::Tab) || Hit(EKeys::Gamepad_Special_Right) || (bStart && MenuPage == 0);
	if (MenuPage == 1)
	{
		// the next button pressed becomes the action being asked for
		if (Hit(EKeys::Tab) || Hit(EKeys::Escape)) { MenuPage = 0; LoadBinds(); return; }
		bool bGot = false;
		if (PadCount > 0) { for (int32 b = 0; b < 32 && !bGot; b++) { if ((Fresh >> b) & 1u) { PadBind[BindAt] = b; bGot = true; } } }
		else
		{
			static const FKey Pad[] = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder,
				EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_RightTrigger, EKeys::Gamepad_LeftThumbstick, EKeys::Gamepad_RightThumbstick };
			for (const FKey& Key : Pad) { if (Hit(Key)) { Bind[BindAt] = Key; bGot = true; break; } }
		}
		if (bGot && ++BindAt >= 8) { BindAt = 0; MenuPage = 0; SaveBinds(); }
		return;
	}
	if (Screen == 3 && !bMenu)
	{
		if (Hit(EKeys::R) || Hit(EKeys::Gamepad_Special_Left) || bSelect) { Restart(); }
		if (Hit(EKeys::B)) { bBoxes = !bBoxes; }
		if (bToggle) { bMenu = true; MenuAt = 0; MenuPage = 0; }
		return;
	}
	const bool bUp = Hit(EKeys::W) || Hit(EKeys::Up) || Hit(EKeys::Gamepad_DPad_Up) || (FreshDir & gk::IN_U), bDown = Hit(EKeys::S) || Hit(EKeys::Down) || Hit(EKeys::Gamepad_DPad_Down) || (FreshDir & gk::IN_D);
	const bool bNext = Hit(EKeys::D) || Hit(EKeys::Right) || Hit(EKeys::Gamepad_DPad_Right) || (FreshDir & gk::IN_R), bPrev = Hit(EKeys::A) || Hit(EKeys::Left) || Hit(EKeys::Gamepad_DPad_Left) || (FreshDir & gk::IN_L);
	const bool bOk = Hit(EKeys::Enter) || Hit(EKeys::J) || Hit(EKeys::Gamepad_FaceButton_Bottom) || Raw(0) || Raw(1) || (bStart && Screen != 3);
	const bool bBack = Hit(EKeys::Escape) || Hit(EKeys::BackSpace) || Hit(EKeys::L) || Hit(EKeys::Gamepad_FaceButton_Right) || Raw(2);
	const int32 Way = bPrev ? -1 : ((bNext || bOk) ? 1 : 0);
	auto Move = [&](int32 Count) { if (bUp) { MenuAt = (MenuAt + Count - 1) % Count; } if (bDown) { MenuAt = (MenuAt + 1) % Count; } };
	if (Screen == 4)
	{   // the animation lab: the pointer does everything, and so do the keys (arrows, ENTER, TAB for the next page, ESC back)
		TArray<int32> Show; if (bLabDrop) { for (int32 k = 0; k < LabList.Num(); k++) { if (LabList[k].Cat == LabDropCat) { Show.Add(k); } } }
		const bool bDownNow = PC->IsInputKeyDown(EKeys::LeftMouseButton), bClick = Hit(EKeys::LeftMouseButton) || (bDownNow && !bLabMouse); bLabMouse = bDownNow;
		if (Hit(EKeys::Escape)) { if (bLabDrop) { bLabDrop = false; } else { LabClose(); return; } }
		if (Hit(EKeys::MouseScrollDown)) { LabScroll += 3; } if (Hit(EKeys::MouseScrollUp)) { LabScroll = FMath::Max(0, LabScroll - 3); }
		if (bClick) { LabClick(LabHot); LabKey = -2; }
		else if (bLabDrop)
		{
			if (bUp) { LabKey = FMath::Max(-1, (LabKey < -1 ? 0 : LabKey) - 1); } if (bDown) { LabKey = FMath::Min(Show.Num() - 1, (LabKey < -1 ? -2 : LabKey) + 1); }
			if (bPrev || bNext) { LabDropCat = (LabDropCat + (bNext ? 1 : 8)) % 9; LabScroll = 0; LabKey = -1; }
			if (LabKey >= 0) { if (LabKey < LabScroll) { LabScroll = LabKey; } if (LabKey >= LabScroll + 12) { LabScroll = LabKey - 11; } }
			if (Hit(EKeys::Enter) && LabKey >= -1) { LabPick(Show.IsValidIndex(LabKey) ? Show[LabKey] : -1); }
		}
		else
		{
			// the cells of the page in view, three to a row
			TArray<int32> Cells; for (int32 k = 0; k < LabSlots.Num(); k++) { if (LabSlots[k].Cat == LabCat) { Cells.Add(k); } }
			int32 At2 = FMath::Max(0, Cells.Find(LabSlot));
			if (bNext) { At2++; } if (bPrev) { At2--; } if (bDown) { At2 += 3; } if (bUp) { At2 -= 3; }
			if (Cells.Num() > 0 && (bNext || bPrev || bDown || bUp)) { LabSlot = Cells[FMath::Clamp(At2, 0, Cells.Num() - 1)]; LabFrame = 0; LabClock = 0.f; }
			if (Hit(EKeys::Tab)) { LabClick(100 + (LabCat + 1) % 9); }
			if (Hit(EKeys::Enter)) { LabClick(1000 + LabSlot); LabKey = -1; }
		}
		// the clip under the pointer (or the keys) in the open list is tried on the fighter straight away
		const int32 Over = !bLabDrop ? -2 : LabHot >= 9999 ? LabHot - 10000 : LabKey >= -1 ? (Show.IsValidIndex(LabKey) ? Show[LabKey] : -1) : -2;
		if (Over != LabTryAt)
		{
			LabTryAt = Over; LabTry = FGkClip(); LabClock = 0.f;
			if (LabList.IsValidIndex(Over)) { LabTry.Anim = LoadObject<UAnimSequence>(nullptr, *LabList[Over].Path); if (LabTry.Anim) { LabTry.Anim->bForceRootLock = true; ClipKeep.Add(LabTry.Anim); } }
		}
		return;
	}
	if (Screen == 0)
	{   // the front menu: VERSUS CPU, TRAINING, WATCH, CPU LEVEL, SET UP BUTTONS, QUIT
		Move(7);
		if (MenuAt == 6 && bOk) { LabOpen(); return; }
		if (MenuAt <= 2 && bOk) { Mode = MenuAt; Setup.Mode = Mode; SelWho = 0; Screen = 1; MenuAt = 0; }
		else if (MenuAt == 3 && Way != 0) { Level = 1 + (Level - 1 + Way + 3) % 3; Setup.Level = Level; gk::aiMake(Cpu[0], Level); gk::aiMake(Cpu[1], Level); }
		else if (MenuAt == 4 && bOk) { MenuPage = 1; BindAt = 0; }
		else if (MenuAt == 5 && bOk) { UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false); }
	}
	else if (Screen == 1)
	{   // choosing fighters: player one, then the other side
		if (bPrev || bNext) { Setup.Char[SelWho] = (Setup.Char[SelWho] + (bPrev ? 3 : 1)) % 4; DressFighter(0); DressFighter(1); }
		if (bOk) { if (SelWho == 0) { SelWho = 1; } else { Screen = 2; StageAt = Mode == 1 ? 0 : FMath::Max(1, Setup.Stage); } }
		else if (bBack) { if (SelWho == 1) { SelWho = 0; } else { Screen = 0; MenuAt = Mode; } }
	}
	else if (Screen == 2)
	{
		const int32 N = Stages.Num();
		if (bUp || bPrev) { StageAt = (StageAt + N - 1) % N; } if (bDown || bNext) { StageAt = (StageAt + 1) % N; }
		if (bOk) { StartStage(StageAt); } else if (bBack) { Screen = 1; SelWho = 1; }
	}
	else
	{   // the pause menu: RESUME, RESTART MATCH, CHARACTER SELECT, STAGE SELECT, MAIN MENU, TRAINING DUMMY, HIT BOXES, SET UP BUTTONS
		if (bToggle || bBack) { bMenu = false; return; }
		Move(8);
		switch (MenuAt)
		{
			case 0: if (bOk) { bMenu = false; } break;
			case 1: if (bOk) { Restart(); bMenu = false; } break;
			case 2: if (bOk) { bMenu = false; Screen = 1; SelWho = 0; } break;
			case 3: if (bOk) { bMenu = false; Screen = 2; StageAt = Setup.Stage; } break;
			case 4: if (bOk) { bMenu = false; Screen = 0; MenuAt = 0; } break;
			case 5: if (Way != 0) { Dummy = (Dummy + Way + 5) % 5; Setup.Dummy = Dummy; } break;
			case 6: if (Way != 0) { bBoxes = !bBoxes; } break;
			default: if (bOk) { MenuPage = 1; BindAt = 0; } break;
		}
	}
}


// ------------------------------------------------------------------------------------------------ running the match
void AGkMatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// the match keeps real time whatever the picture does: a slow frame drops pictures, never speed
	const double Now = FPlatformTime::Seconds(); const double Real = LastReal > 0.0 ? FMath::Clamp(Now - LastReal, 0.0, 0.25) : double(DeltaSeconds); LastReal = Now;
	Acc += Real; FrameMs = FrameMs * 0.9f + float(Real) * 100.f;
	PadPrev = PadNow; PadDirPrev = PadDirs; PadNames.Reset(); PadCount = Pads ? Pads->Read(PadNow, PadDirs, &PadNames) : 0;
	// a pack's map streams in around the camera first; the fight is set down in it once its ground is there
	if (!bPlaced) { PlaceWait += float(Real); if (PlaceWait > 0.6f && TryPlace()) { bPlaced = true; Restart(); CamD.bSnap = true; if (bScript) { Screen = 3; for (int32 k = 0; k < 400 && S.phase != gk::PH_FIGHT; k++) { gk::step(S, 0, 0); } } } }
	if ((++LightsClock % 30) == 1 && LightsClock < 1800) { if (Kind == 2) { QuietMapLights(); } if (bPlaced && Kind != 0) { if (!bNinjaTried) { NinjaFog(); } Mist(); } }
	if (bScript)
	{
		Acc = 0.0;
		Ticks++;
		RunScript();
		Present(float(Real));
		if (!StatusPath.IsEmpty() && (Ticks % 30) == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("GkCapture frame %.2f ms, storm %.2f, render scale %.0f%%"), FrameMs, Storm, ScreenPct);
		}
		return;
	}
	InputNow = ReadPlayer();
	MenuInput();
	const double Step = 1.0 / 60.0, T0 = FPlatformTime::Seconds(); int32 Steps = 0;
	while (Acc >= Step && Steps < 8)
	{
		Acc -= Step; Steps++; Ticks++;
		if (Screen == 4) { const int32 Count = LabSlots.IsValidIndex(LabSlot) ? FMath::Max(1, Poses.Length(LabSlots[LabSlot].Slot)) : 1; LabFrame = (LabFrame + 1) % (Count + 25); LabClock += 1.f / 60.f; if (LabNoteTimer > 0) { LabNoteTimer--; } }
		else if (!bMenu && bPlaced) { StepOnce(); }
		AnimClock++;
	}
	if (Acc > Step) { Acc = 0.0; }
	StepMs = StepMs * 0.9f + float(FPlatformTime::Seconds() - T0) * 100.f;
	Present(float(Real));
	if (!StatusPath.IsEmpty() && (++StatusClock % 30) == 0)
	{
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("fps %d storm %.2f weather %d screen %d stage %d kind %d placed %d at %s map %s\n%s\nbuttons %08x dirs %d input %d\nframe %d menu %d\n"), FMath::RoundToInt(1000.f / FMath::Max(1.f, FrameMs)), Storm, WeatherAt, Screen, Setup.Stage, Kind, bPlaced ? 1 : 0, *PlaceNote, *MapName(), Pads ? *Pads->Debug() : TEXT(""), PadNow, PadDirs, InputNow, S.frame, bMenu ? 1 : 0), *StatusPath);
	}
}

void AGkMatch::StepOnce(int32 Force0, int32 Force1)
{
	const int32 Human = InputNow; int32 I0 = Human, I1 = 0;
	if (Force0 >= 0) { I0 = Force0; I1 = FMath::Max(0, Force1); }
	else if (Mode == 2 || Screen != 3) { I0 = gk::aiThink(Cpu[0], S, 0); I1 = gk::aiThink(Cpu[1], S, 1); }   // behind the menus the CPU fights itself
	else if (Mode == 1) { I1 = DummyInput(); }
	else { I1 = gk::aiThink(Cpu[1], S, 1); }
	gk::step(S, I0, I1);
	for (int32 e = 0; e < S.nev; e++) { OnEvent(S.ev[e]); }
	if (Mode == 1 && Screen == 3) { Track(I0); }
	if (CallHold > 0 && ++CallAge > CallHold) { CallHold = 0; }
	for (int32 i = 0; i < 2; i++) { if (ComboTimer[i] > 0) { ComboTimer[i]--; } if (NoteTimer[i] > 0) { NoteTimer[i]--; } }
	for (int32 k = Sparks.Num() - 1; k >= 0; k--) { if (++Sparks[k].Age > 12) { Sparks.RemoveAtSwap(k); } }
	for (int32 i = 0; i < 2; i++) { if (ImpactT[i] > 0) { ImpactT[i]--; } if (PerfectT[i] > 0) { PerfectT[i]--; } Lean[i] *= 0.84f; Squash[i] *= 0.72f; StateFx(i); }
	// the match is decided: any attack button, or a few seconds, starts the next
	if (S.phase == gk::PH_OVER) { if (++OverTimer > 420 || (OverTimer > 50 && Screen == 3 && (Human & 0xFF0))) { Restart(); } } else { OverTimer = 0; }
}

/** Training: the last inputs with how long each was held, and who recovered first after the last exchange (as the browser shows it). */
void AGkMatch::Track(int32 In)
{
	if (In != LastIn) { Hist.Insert(FHist{ In, 1 }, 0); if (Hist.Num() > 12) { Hist.Pop(); } LastIn = In; } else if (Hist.Num() > 0 && Hist[0].Frames < 99) { Hist[0].Frames++; }
	for (int32 i = 0; i < 2; i++) { const bool bIdle = S.p[i].st == gk::IDLE; if (bIdle && !bWasIdle[i]) { ActFrame[i] = S.frame; } bWasIdle[i] = bIdle; }
	if (S.p[1].st == gk::HIT || S.p[1].st == gk::BLOCK) { bExchange = true; }
	if (bExchange && bWasIdle[0] && bWasIdle[1]) { Adv = ActFrame[1] - ActFrame[0]; bExchange = false; }
	if (S.p[1].st == gk::AHIT || S.p[1].st == gk::KDOWN) { bExchange = false; }
}

/** One of a sound's takes, at random (the old-school shonen pack: every sound comes as six takes). */
void AGkMatch::Snd(const TCHAR* Set, float Volume, float Pitch)
{
	if (Screen != 3 || bMenu) { return; }
	if (const TArray<USoundBase*>* Takes = SndSets.Find(FName(Set))) { if (Takes->Num() > 0) { UGameplayStatics::PlaySound2D(this, (*Takes)[Fx.Rng.RandRange(0, Takes->Num() - 1)], Volume * 0.8f, Pitch * Fx.Rng.FRandRange(0.96f, 1.04f)); } }
}

/** What each thing that happens in the fight sounds like. */
void AGkMatch::EventSound(const gk::Event& E)
{
	const int32 W = E.who >= 0 ? E.who & 1 : 0; const gk::Fighter& P = S.p[W];
	switch (E.type)
	{
		case gk::E_HIT:
			if (E.f & gk::H_KO) { Snd(TEXT("Explosion_05"), 1.f); Snd(TEXT("Melee_04"), 1.f); Snd(TEXT("Vanish_01"), 0.8f); Snd(TEXT("Damaged_01"), 0.5f); }
			else if (E.f & gk::H_FINISH) { Snd(TEXT("Explosion_04"), 0.9f); Snd(TEXT("Melee_09"), 1.f); }
			else if (E.f & gk::H_THROW) { Snd(TEXT("Melee_03"), 1.f); Snd(TEXT("Land_04"), 0.8f); }
			else if (E.f & gk::H_IMPACT) { Snd(TEXT("Melee_01"), 1.f); Snd(TEXT("Explosion_01"), 0.5f); }
			else if (E.e) { Snd(E.a >= 2 ? TEXT("Melee_04") : TEXT("Melee_09"), 1.f); Snd(TEXT("ImpactBurst_01"), 0.55f); }   // a counter or a punish rings as well
			else { static const TCHAR* Lv[3] = { TEXT("Melee_06"), TEXT("Melee_05"), TEXT("Melee_04") }; static const TCHAR* Alt[3] = { TEXT("Melee_07"), TEXT("Melee_10"), TEXT("Melee_03") };
				const int32 L = FMath::Clamp(E.a, 0, 2); Snd((Ticks & 1) ? Lv[L] : Alt[L], 0.7f + 0.15f * L); if (L == 2) { Snd(TEXT("Damaged_01"), 0.3f); } }
			break;
		case gk::E_BLOCK: Snd((Ticks & 1) ? TEXT("Land_06") : TEXT("Land_07"), 0.6f, 1.25f); break;
		case gk::E_PARRY: Snd(TEXT("AbilityReady_06"), 0.8f); break;
		case gk::E_PPARRY: Snd(TEXT("AbilityReady_04"), 1.f); Snd(TEXT("Spark_02"), 0.7f); Snd(TEXT("ImpactBurst_01"), 0.7f); break;
		case gk::E_PARRYON: Snd(TEXT("AbilityReady_02"), 0.7f); break;
		case gk::E_ARMOR: Snd(TEXT("Melee_11"), 0.9f); break;
		case gk::E_DICLASH: case gk::E_CLASH: Snd(TEXT("Melee_01"), 1.f); Snd(TEXT("Spark_01"), 0.8f); break;
		case gk::E_SPLAT: case gk::E_WBOUNCE: Snd(TEXT("Land_11"), 1.f); Snd(TEXT("Melee_11"), 0.8f); break;
		case gk::E_GBOUNCE: Snd(TEXT("Land_11"), 0.9f); break;
		case gk::E_KD: Snd((Ticks & 1) ? TEXT("Land_03") : TEXT("Land_04"), 0.85f); Snd(TEXT("Step_01"), 0.4f, 0.8f); break;
		case gk::E_LAND: Snd((Ticks & 1) ? TEXT("Land_08") : TEXT("Land_01"), 0.4f); Snd(TEXT("Step_01"), 0.3f); break;
		case gk::E_JUMP: case gk::E_WALLJUMP: Snd((Ticks & 1) ? TEXT("Jump_05") : TEXT("Jump_01"), 0.55f); break;
		case gk::E_DASH: Snd((Ticks & 1) ? TEXT("Dodge_02") : TEXT("Dodge_08"), 0.6f); break;
		case gk::E_RUN: Snd(TEXT("Dodge_09"), 0.5f); break;
		case gk::E_RUSH: Snd(TEXT("Fly_01"), 0.7f); Snd(TEXT("Zap_01"), 0.6f); break;
		case gk::E_HOME: Snd((Ticks & 1) ? TEXT("Fly_04") : TEXT("Fly_05"), 0.7f); Snd(TEXT("Flyby_01"), 0.45f); break;
		case gk::E_RISE: Snd(TEXT("Jump_01"), 0.6f); Snd(TEXT("Launch_01"), 0.55f); break;
		case gk::E_TECH: Snd(TEXT("Dodge_08"), 0.7f); break;
		case gk::E_WHIFF: { static const TCHAR* Sw[3] = { TEXT("Swing_03"), TEXT("Swing_02"), TEXT("Swing_01") }; static const TCHAR* Sw2[3] = { TEXT("Swing_04"), TEXT("Swing_05"), TEXT("Swing_01") };
			const int32 L = FMath::Clamp(E.a, 0, 2); Snd((Ticks & 1) ? Sw[L] : Sw2[L], 0.45f + 0.12f * L); } break;
		case gk::E_GRAB: Snd(TEXT("Throw_02"), 1.f); Snd(TEXT("Throw_04"), 0.8f); break;
		case gk::E_FIRE: if (P.st == gk::ATK && P.mv == gk::moveByKey("HAD_OD")) { Snd(TEXT("AbilityRelease_13"), 0.9f); Snd(TEXT("Laser_01"), 0.6f); } else { Snd((Ticks & 1) ? TEXT("AbilityRelease_06") : TEXT("AbilityRelease_03"), 0.8f); Snd(TEXT("LaserShot_01"), 0.5f); } break;
		case gk::E_SPECIAL: Snd(TEXT("AbilityReady_05"), 0.7f); break;
		case gk::E_EX: Snd(TEXT("AbilityReady_03"), 0.8f); break;
		case gk::E_DI: Snd(TEXT("AbilityRelease_14"), 0.9f); break;
		case gk::E_SUPER: Snd(TEXT("AbilityCharge_11"), 1.f); Snd(TEXT("WindUp_01"), E.c == gk::moveByKey("SA3") ? 0.9f : 0.5f); break;
		case gk::E_BURNOUT: Snd(TEXT("Malfunction_01"), 0.8f); Snd(TEXT("Spark_01"), 0.5f); break;
		case gk::E_RECOVER: Snd(TEXT("Power_01"), 0.6f); break;
		case gk::E_ROUND: Snd(TEXT("Power_01"), 0.5f); break;
		case gk::E_FIGHT: Snd(TEXT("AbilityReady_03"), 0.9f); break;
		case gk::E_KO: Snd(TEXT("Explosion_04"), 0.8f); break;
		default: break;
	}
}

void AGkMatch::OnEvent(const gk::Event& E)
{
	EventSound(E);
	const int32 Who = E.who >= 0 ? (E.who & 1) : 0;
	const FVector Feet = At(S.p[Who].x, S.p[Who].y); const FLinearColor Ki = Who ? FLinearColor(1.5f, 3.5f, 8.f) : FLinearColor(7.f, 3.6f, 0.9f);
	switch (E.type)
	{
		case gk::E_HIT:
			HitKind[1 - Who] = (E.f & gk::H_LOW) ? 3 : E.a >= 2 ? 2 : E.a >= 1 ? 1 : 0;   // how hard it landed picks the reel: light, medium, heavy, or swept low
			{   // one spark and one sound for each blow that lands, sized by how hard it was
				const int32 Lv = (E.f & gk::H_KO) ? 3 : FMath::Clamp(E.a, 0, 2); const float Dr = float(S.p[Who].face); const FVector Pt = At(E.x, E.y, -30.0);
				static const TCHAR* Sys[4] = { TEXT("NS_Hit_01_Once"), TEXT("NS_Hit_04_Once"), TEXT("NS_Hit_06_Directional_Once"), TEXT("NS_Radial_Spiky_Hit_01") };
				static const float Size[4] = { 0.55f, 0.85f, 1.2f, 1.6f };
				if (!PlayVfx(Sys[Lv], Pt, Size[Lv], Lv == 2 ? Dr : 0.f, nullptr)) { Fx.Hit(At(E.x, E.y), Lv, Dr); }
			}
			Shake = FMath::Max(Shake, 2.f + E.a * 2.f + ((E.f & gk::H_KO) ? 10.f : 0.f));
			{ const float Dr = float(S.p[Who].face); const bool bKo = (E.f & gk::H_KO) != 0, bFin = (E.f & gk::H_FINISH) != 0;
			  CamKick(Dr * (bKo ? 8.f : bFin ? 9.f : E.a >= 2 ? 3.f : 1.2f), bKo ? -0.14f : bFin ? -0.16f : E.a >= 2 ? -0.05f : -0.02f, bKo ? 12.f : bFin ? 14.f : E.a >= 2 ? 4.f : 0.f, bKo ? Dr * 8.f : bFin ? Dr * 6.f : 0.f); }
			Combo[Who] = E.c; ComboDmg[Who] = E.d; ComboTimer[Who] = 80;
			Lean[1 - Who] = (E.f & gk::H_KO) ? 22.f : 6.f + 5.f * FMath::Clamp(E.a, 0, 2); Flash = FMath::Max(Flash, (E.f & gk::H_KO) ? 1.f : E.a >= 2 ? 0.22f : 0.f);
			if (E.f & gk::H_SPECIAL) { Shake = FMath::Max(Shake, 7.f); Flash = FMath::Max(Flash, 0.3f); }
			if ((E.f & gk::H_FINISH) && S.p[Who].st == gk::ATK && S.p[Who].mv >= 0 && gk::move(S.p[Who].mv).sup)
			{   // a super ends on the kit's beam, fired from the chest through the other fighter, and a shockwave under him
				const gk::Fighter& Lp = S.p[1 - Who];
				PlayVfx(TEXT("NS_Ground_ShockWave"), At(Lp.x, 0, -10.0), 1.6f, 0.f, nullptr); PlayVfx(TEXT("NS_Thunder"), At(Lp.x, Lp.y, -10.0), 1.4f, 0.f, nullptr);
			}
			if (E.f & gk::H_IMPACT) { Remark(Who, (E.f & gk::H_CRUMPLE) ? TEXT("CRUMPLE!") : TEXT("BREACH!")); }
			else if (E.f & gk::H_FREE) { Remark(Who, TEXT("CHASE")); }
			else if (E.f & gk::H_LOW) { Remark(Who, TEXT("LOW!")); }
			else if (E.f & gk::H_OVERHEAD) { Remark(Who, TEXT("OVERHEAD!")); }
			else if (E.e == 2 && E.c <= 1) { Remark(Who, TEXT("PUNISH")); }
			else if (E.e == 1 && E.c <= 1) { Remark(Who, TEXT("COUNTER")); }
			break;
		case gk::E_BLOCK: CamKick(float(S.p[Who].face) * 0.8f, -0.012f, 0.f); if (!PlayVfx(TEXT("NS_Block_Radial_01_Once"), At(E.x, E.y, -30.0), 0.38f, 0.f, TEXT("SFX_Vefects_Block_01_Cue"))) { Fx.Guard(At(E.x, E.y), float(S.p[Who].face)); } Shake = FMath::Max(Shake, 1.5f);
			Fx.Add(0, At(E.x, E.y, -30.0), FVector::ZeroVector, 0.16f, 60.f, 150.f, FLinearColor(0.25f, 0.7f, 2.6f)); break;
		case gk::E_PARRY: Fx.Parry(At(E.x, E.y), false); break;
		case gk::E_PPARRY: Fx.Parry(At(E.x, E.y), true); Shake = FMath::Max(Shake, 6.f); Remark(Who, TEXT("PERFECT PARRY")); PerfectT[Who] = 50; Flash = FMath::Max(Flash, 0.7f);
			Fx.Add(0, At(E.x, E.y, -30.0), FVector::ZeroVector, 0.35f, 120.f, 620.f, FLinearColor(8.f, 6.f, 1.5f)); Fx.Aura(Feet, FLinearColor(8.f, 6.f, 1.5f), 1.2f); break;
		case gk::E_ARMOR: Fx.Guard(At(E.x, E.y), float(S.p[Who].face)); Remark(Who, TEXT("ARMOR")); break;
		case gk::E_DICLASH: CamKick(7.f, -0.14f, 12.f); Say(TEXT("CLASH!"), 50); Shake = FMath::Max(Shake, 12.f); break;
		case gk::E_SPLAT: CamKick(Feet.Y > 0 ? 6.f : -6.f, -0.1f, 8.f, Feet.Y > 0 ? 8.f : -8.f); Fx.Hit(Feet + FVector(0, 0, 100), 2, float(-S.p[Who].face)); Fx.Dust(Feet, 1.4f, 0.f); Remark(Who, E.a ? TEXT("STUN!") : TEXT("WALL SPLAT")); Shake = FMath::Max(Shake, 12.f); break;
		case gk::E_GBOUNCE: case gk::E_WBOUNCE: if (E.type == gk::E_WBOUNCE) { CamKick(Feet.Y > 0 ? 5.f : -5.f, -0.1f, 8.f, Feet.Y > 0 ? 7.f : -7.f); } else { CamKick((Ticks & 1) ? 4.f : -4.f, -0.08f, 7.f); } Fx.Dust(Feet, 1.6f, 0.f); Fx.Hit(FVector(Feet.X, Feet.Y, E.type == gk::E_GBOUNCE ? 10.0 : Feet.Z + 90.0), 1, 0.f); Shake = FMath::Max(Shake, 10.f); break;
		case gk::E_KD: Fx.Dust(Feet, 1.3f, 0.f); Shake = FMath::Max(Shake, 3.f); break;
		case gk::E_LAND: Squash[Who] = 1.f; if (!PlayVfx(TEXT("NS_Directional_Dust_01_Once"), Feet + FVector(-20, 0, 2), 0.2f, 0.f, TEXT("SFX_Vefects_Directional_Dust_01_Cue"), 0.5f)) { Fx.Dust(Feet, 0.55f, 0.f); } break;
		case gk::E_JUMP: case gk::E_WALLJUMP: if (!PlayVfx(TEXT("NS_Directional_Dust_Jump_01_Once"), FVector(Feet.X - 20, Feet.Y, 2), 0.2f, 0.f, TEXT("SFX_Vefects_Jump_Dust_01_Cue"), 0.6f)) { Fx.Dust(Feet, 0.45f, 0.f); } break;
		case gk::E_TECH: case gk::E_RISE: Fx.Dust(Feet, 0.45f, 0.f); break;
		case gk::E_DASH: case gk::E_RUN: if (!PlayVfx(S.p[Who].face > 0 ? TEXT("NS_Directional_Dust_03_Once_Fast") : TEXT("NS_Directional_Dust_03_Once_Fast_Flip"), FVector(Feet.X - 20, Feet.Y, 2), 0.34f, 0.f, nullptr)) { Fx.Dust(Feet, 0.8f, float(S.p[Who].face)); } break;
		case gk::E_DI:
		{   // a drive impact is announced: the screen kicks in on him, a burst of paint-bright streaks, his name for it over his head
			const float Dr = float(S.p[Who].face); const FVector Ch = Feet + FVector(-20.0, 0.0, 95.0);
			CamKick(Dr * 5.f, -0.2f, 16.f, Dr * 5.f); Shake = FMath::Max(Shake, 7.f); Flash = FMath::Max(Flash, 0.45f); Remark(Who, TEXT("DRIVE IMPACT")); ImpactT[Who] = 40;
			static const FLinearColor Paint[4] = { FLinearColor(9.f, 0.6f, 5.f), FLinearColor(0.5f, 6.f, 8.f), FLinearColor(9.f, 6.f, 0.5f), FLinearColor(4.f, 0.8f, 9.f) };
			for (int32 k = 0; k < 28; k++)
			{
				const float A = Fx.Rng.FRandRange(-1.2f, 1.2f), Sp = Fx.Rng.FRandRange(700.f, 1900.f);
				FGkFx::FP& St = Fx.Add(0, Ch, FVector(0.0, -Dr * FMath::Cos(A) * Sp, FMath::Sin(A) * Sp), Fx.Rng.FRandRange(0.25f, 0.5f), Fx.Rng.FRandRange(14.f, 30.f), 4.f, Paint[k & 3]); St.Stretch = 10.f; St.Drag = 3.f;
			}
			Fx.Add(0, Ch, FVector::ZeroVector, 0.3f, 80.f, 520.f, FLinearColor(5.f, 0.5f, 3.5f)); Fx.Aura(Feet, FLinearColor(8.f, 0.8f, 5.f), 1.8f);
			Snd(TEXT("ImpactBurst_01"), 0.9f); Snd(TEXT("WindUp_01"), 0.6f, 1.4f);
			break;
		}
		case gk::E_EX: case gk::E_HOME: if (E.type == gk::E_HOME) { CamKick(0.f, 0.f, 9.f); } Fx.Aura(Feet, Ki, 0.7f); break;
		case gk::E_RUSH: Fx.Aura(Feet, Ki, 0.9f); Remark(Who, TEXT("GHOST STEP")); break;
		case gk::E_BURNOUT: Remark(Who, TEXT("BURNOUT")); Say(TEXT("BURNOUT"), 45); Shake = FMath::Max(Shake, 8.f); Fx.Aura(Feet, FLinearColor(0.5f, 0.55f, 0.5f), 1.3f); Fx.Dust(Feet, 1.6f, 0.f); break;
		case gk::E_SPECIAL: PlayVfx(TEXT("NS_Aura2"), Feet + FVector(-10.0, 0.0, 0.0), 1.f, 0.f, nullptr); Fx.Aura(Feet, Ki, 0.9f); CamKick(0.f, -0.04f, 6.f); Shake = FMath::Max(Shake, 2.5f); break;
		case gk::E_SUPER: PlayVfx(TEXT("NS_Aura"), Feet + FVector(-10.0, 0.0, 0.0), 1.5f, 0.f, nullptr); Fx.Aura(Feet, Ki, 2.6f); Flash = FMath::Max(Flash, 0.9f); BoltDue = 0.35f; Say(*FString(UTF8_TO_TCHAR(gk::moveName(E.c))).ToUpper(), 70); Shake = FMath::Max(Shake, 6.f); break;
		case gk::E_ROUND:
		{
			const bool bFinal = E.a > 1 && E.a >= S.cfg.rounds * 2 - 1 && S.p[0].wins == S.p[1].wins;
			if (bFinal) { Say(TEXT("FINAL ROUND"), 80); } else { Say(*FString::Printf(TEXT("ROUND %d"), E.a), 80); }
			break;
		}
		case gk::E_FIGHT: Say(TEXT("FIGHT!"), 50); break;
		case gk::E_KO: Say(E.a == gk::KO_TIME ? TEXT("TIME OVER") : E.a == gk::KO_DOUBLE ? TEXT("DOUBLE K.O.") : E.a == gk::KO_DRAW ? TEXT("DRAW") : E.a == gk::KO_PERFECT ? TEXT("PERFECT K.O.") : TEXT("K.O."), 170); break;
		default: break;
	}
}

// ------------------------------------------------------------------------------------------------ the picture
namespace
{
	/** Poses made by hand for what no bought clip covers: the win, the wall splat, the grab and the four throws, landing, the jump's start, the crouching guard.
	    Each is a few key poses (frame, then the solver's 25 numbers) eased from one to the next. */
	struct FOwnKey { float At; float V[25]; };
	const float* GkOwnPose(FName Clip, float T)
	{
		static TMap<FName, TArray<FOwnKey>> All; static float Out[25];
		if (All.Num() == 0)
		{
			auto K = [](const TCHAR* Name, float At, float hx, float hy, float lean, float up, float tw, float pt, float head, float eL, float eR, float kL, float kR,
				float lx, float ly, float lz, float rx, float ry, float rz, float flx, float fly, float flz, float frx, float fry, float frz)
			{ FOwnKey Key = { At, { hx, hy, lean, up, tw, pt, head, eL, eR, kL, kR, 0.f, 0.f, lx, ly, lz, rx, ry, rz, flx, fly, flz, frx, fry, frz } }; All.FindOrAdd(FName(Name)).Add(Key); };
#define GK_IDLE -5.1f, 45.7f, -13.f, -6.6f, -68.f, -42.f, 9.5f, -0.7f, -0.5f, 0.1f, 0.6f, 14.6f, 73.6f, 0.f, -15.6f, 35.2f, 0.f, 9.7f, 7.4f, 0.f, -12.1f, 4.f, 0.f
#define GK_CLINCH 4.f, 43.f, 12.f, 18.f, -46.f, -42.f, -8.f, -0.2f, -0.2f, 0.2f, 0.5f, 28.f, 68.f, -5.f, 25.f, 58.f, 5.f, 18.f, 4.f, 0.f, -15.f, 4.f, 0.f
			// the win: he drops his weight, then throws a fist at the sky and holds it
			K(TEXT("win"), 0, GK_IDLE);
			K(TEXT("win"), 12, -2.f, 36.f, 12.f, 20.f, -60.f, -42.f, -12.f, -0.6f, -0.6f, 0.6f, 0.8f, 8.f, 42.f, 2.f, -8.f, 34.f, -2.f, 11.f, 4.f, 3.f, -13.f, 4.f, -3.f);
			K(TEXT("win"), 22, 0.f, 48.f, -6.f, -12.f, -84.f, -64.f, 18.f, 0.4f, -0.8f, 0.3f, 0.6f, 4.f, 113.f, 4.f, -10.f, 46.f, -4.f, 10.f, 4.f, 7.f, -12.f, 4.f, -7.f);
			K(TEXT("win"), 40, 0.f, 47.f, -4.f, -9.f, -84.f, -64.f, 14.f, 0.4f, -0.8f, 0.3f, 0.6f, 5.f, 110.f, 4.f, -10.f, 45.f, -4.f, 10.f, 4.f, 7.f, -12.f, 4.f, -7.f);
			// against the wall: flat, arms flung up and wide, head snapped back, then sagging
			K(TEXT("splat"), 0, -6.f, 48.f, -10.f, -16.f, -90.f, -88.f, -30.f, 1.2f, 1.2f, 0.2f, 0.2f, 32.f, 100.f, -6.f, -42.f, 96.f, -6.f, 14.f, 12.f, 0.f, -24.f, 9.f, 0.f);
			K(TEXT("splat"), 26, -6.f, 40.f, -4.f, -6.f, -90.f, -88.f, 18.f, 0.4f, 0.4f, 0.4f, 0.4f, 26.f, 70.f, -6.f, -36.f, 66.f, -6.f, 12.f, 4.f, 0.f, -22.f, 4.f, 0.f);
			// the grab: a lunge with both hands, into a clinch
			K(TEXT("grab"), 0, GK_IDLE);
			K(TEXT("grab"), 5, 8.f, 41.f, 18.f, 24.f, -38.f, -42.f, -12.f, -0.1f, -0.1f, 0.2f, 0.5f, 36.f, 66.f, -7.f, 33.f, 57.f, 7.f, 23.f, 4.f, 0.f, -17.f, 4.f, 0.f);
			K(TEXT("grab"), 10, GK_CLINCH);
			// held: a knee driven up into him
			K(TEXT("bite"), 0, GK_CLINCH);
			K(TEXT("bite"), 4, 6.f, 46.f, -6.f, 4.f, -40.f, -20.f, -4.f, -0.2f, -0.2f, 0.2f, 0.2f, 24.f, 58.f, -5.f, 22.f, 50.f, 5.f, 16.f, 4.f, 0.f, 20.f, 36.f, 0.f);
			K(TEXT("bite"), 9, GK_CLINCH);
			// forward: he hauls him in, turns his hip under and heaves him out past his front foot
			K(TEXT("throw1"), 0, GK_CLINCH);
			K(TEXT("throw1"), 6, -2.f, 38.f, 4.f, 6.f, -8.f, -20.f, 0.f, -0.5f, -0.5f, 0.5f, 0.7f, 14.f, 60.f, -4.f, 12.f, 52.f, 4.f, 14.f, 4.f, 0.f, -14.f, 4.f, 0.f);
			K(TEXT("throw1"), 13, 10.f, 40.f, 30.f, 38.f, -30.f, -42.f, -18.f, 0.f, 0.f, 0.2f, 0.5f, 44.f, 50.f, -6.f, 42.f, 42.f, 6.f, 25.f, 4.f, 0.f, -19.f, 7.f, 0.f);
			K(TEXT("throw1"), 21, 8.f, 41.f, 24.f, 30.f, -36.f, -42.f, -10.f, 0.f, 0.f, 0.2f, 0.5f, 38.f, 36.f, -6.f, 34.f, 30.f, 6.f, 24.f, 4.f, 0.f, -18.f, 5.f, 0.f);
			K(TEXT("throw1"), 30, GK_IDLE);
			// back: he falls away, turning, and slings him over the shoulder behind
			K(TEXT("throw2"), 0, GK_CLINCH);
			K(TEXT("throw2"), 6, -2.f, 38.f, -4.f, -2.f, -30.f, -30.f, 0.f, -0.5f, -0.5f, 0.5f, 0.7f, 14.f, 62.f, -4.f, 12.f, 54.f, 4.f, 14.f, 4.f, 0.f, -14.f, 4.f, 0.f);
			K(TEXT("throw2"), 14, -8.f, 40.f, -26.f, -34.f, -150.f, -110.f, -22.f, 0.5f, 0.5f, 0.3f, 0.5f, -30.f, 72.f, -6.f, -34.f, 64.f, 6.f, 12.f, 4.f, 0.f, -20.f, 4.f, 0.f);
			K(TEXT("throw2"), 22, -6.f, 42.f, -16.f, -22.f, -150.f, -110.f, -10.f, 0.f, 0.f, 0.3f, 0.5f, -38.f, 44.f, -6.f, -40.f, 38.f, 6.f, 12.f, 4.f, 0.f, -19.f, 4.f, 0.f);
			K(TEXT("throw2"), 30, GK_IDLE);
			// up: he dips under and launches him overhead with both arms
			K(TEXT("throw3"), 0, GK_CLINCH);
			K(TEXT("throw3"), 6, 4.f, 30.f, 14.f, 20.f, -50.f, -42.f, -10.f, -0.6f, -0.6f, 0.8f, 0.9f, 22.f, 40.f, -5.f, 20.f, 34.f, 5.f, 16.f, 4.f, 3.f, -14.f, 4.f, -3.f);
			K(TEXT("throw3"), 12, 3.f, 49.f, -8.f, -16.f, -70.f, -50.f, 22.f, 0.5f, 0.5f, 0.2f, 0.5f, 16.f, 112.f, -6.f, 10.f, 110.f, 6.f, 13.f, 4.f, 0.f, -13.f, 6.f, 0.f);
			K(TEXT("throw3"), 22, 2.f, 47.f, -6.f, -12.f, -70.f, -50.f, 18.f, 0.5f, 0.5f, 0.2f, 0.5f, 14.f, 106.f, -6.f, 9.f, 102.f, 6.f, 13.f, 4.f, 0.f, -13.f, 4.f, 0.f);
			K(TEXT("throw3"), 30, GK_IDLE);
			// down: he lifts him and drives him into the floor
			K(TEXT("throw4"), 0, GK_CLINCH);
			K(TEXT("throw4"), 6, 2.f, 47.f, -8.f, -10.f, -50.f, -42.f, 6.f, 0.4f, 0.4f, 0.2f, 0.5f, 22.f, 94.f, -5.f, 19.f, 90.f, 5.f, 15.f, 4.f, 0.f, -14.f, 4.f, 0.f);
			K(TEXT("throw4"), 12, 8.f, 28.f, 34.f, 44.f, -44.f, -42.f, -20.f, -0.3f, -0.3f, 0.8f, 0.9f, 32.f, 12.f, -5.f, 30.f, 8.f, 5.f, 22.f, 4.f, 3.f, -18.f, 4.f, -3.f);
			K(TEXT("throw4"), 22, 8.f, 29.f, 32.f, 40.f, -44.f, -42.f, -16.f, -0.3f, -0.3f, 0.8f, 0.9f, 31.f, 13.f, -5.f, 29.f, 9.f, 5.f, 22.f, 4.f, 3.f, -18.f, 4.f, -3.f);
			K(TEXT("throw4"), 30, GK_IDLE);
			// caught: bent backward off his feet, arms flailing; limp once he is thrown
			K(TEXT("thrown"), 0, 0.f, 46.f, -14.f, -22.f, -66.f, -42.f, -26.f, 0.3f, 0.3f, 0.2f, 0.5f, -4.f, 88.f, -14.f, -14.f, 82.f, 14.f, 9.f, 8.f, 0.f, -6.f, 6.f, 0.f);
			K(TEXT("thrown"), 8, 0.f, 46.f, -28.f, -40.f, -70.f, -42.f, -34.f, 0.6f, 0.6f, 0.4f, 0.6f, -22.f, 92.f, -18.f, -30.f, 84.f, 18.f, 18.f, 26.f, -4.f, 6.f, 16.f, 4.f);
			// landing: the knees take it, hands low for balance, then up
			K(TEXT("land"), 0, 0.f, 29.f, 18.f, 26.f, -60.f, -42.f, -14.f, -0.6f, -0.6f, 0.9f, 1.0f, 16.f, 34.f, -8.f, -10.f, 30.f, 8.f, 13.f, 4.f, 4.f, -13.f, 4.f, -4.f);
			K(TEXT("land"), 6, -3.f, 42.f, -6.f, 0.f, -66.f, -42.f, 4.f, -0.7f, -0.5f, 0.2f, 0.6f, 14.f, 66.f, 0.f, -14.f, 36.f, 0.f, 10.f, 4.f, 0.f, -12.f, 4.f, 0.f);
			// the jump's start: he coils, arms swung back
			K(TEXT("prejump"), 0, GK_IDLE);
			K(TEXT("prejump"), 4, -1.f, 32.f, 14.f, 20.f, -62.f, -42.f, -10.f, -0.6f, -0.6f, 0.7f, 0.8f, -8.f, 34.f, -8.f, -18.f, 30.f, 8.f, 11.f, 4.f, 2.f, -12.f, 4.f, -2.f);
			// the crouching guard: low, both forearms up in front of the face
			K(TEXT("blockC"), 0, -3.f, 20.f, 10.f, 20.f, -56.f, -45.f, -12.f, 0.5f, 0.5f, 1.2f, 1.2f, 13.f, 53.f, 6.f, 12.f, 45.f, -5.f, 8.f, 9.f, 7.f, 0.f, 10.f, -7.f);
#undef GK_IDLE
#undef GK_CLINCH
		}
		const TArray<FOwnKey>* Ks = All.Find(Clip); if (!Ks || Ks->Num() == 0) { return nullptr; }
		const TArray<FOwnKey>& A = *Ks; int32 n = 0; while (n + 1 < A.Num() && T >= A[n + 1].At) { n++; }
		if (n + 1 >= A.Num() || T <= A[n].At) { FMemory::Memcpy(Out, A[T <= A[0].At ? 0 : n].V, sizeof(Out)); return Out; }
		float u = (T - A[n].At) / FMath::Max(0.01f, A[n + 1].At - A[n].At); u = u * u * (3.f - 2.f * u);
		for (int32 k = 0; k < 25; k++) { Out[k] = FMath::Lerp(A[n].V[k], A[n + 1].V[k], u); }
		return Out;
	}
}


/**
 * Shows one fighter on the frame the rules are on: picks the browser game's pose for his state or move at that frame
 * (the same choices the browser's fighterPose makes) and solves it onto the skeleton. Nothing plays by itself.
 */
void AGkMatch::ShowFighter(int32 i)
{
	UPoseableMeshComponent* C = Body[i]; if (!C) { return; }
	const gk::Fighter& P = S.p[i]; const gk::Fighter& O = S.p[1 - i];
	FName Clip(TEXT("idle")); int32 T = (S.frame + i * 44) % 262; float Roll = 0.f, Turn = 0.f;
	const bool bLab = Screen == 4; int32 LabMove = -1, MoveId = -1, Raw = P.sf;
	if (bLab)
	{
		if (i == 1) { C->SetVisibility(false); if (Anim[1]) { Anim[1]->SetVisibility(false); } return; }
		if (bScript && !bLabUi) { Clip = ScriptSlot; } else if (LabSlots.IsValidIndex(LabSlot)) { Clip = LabSlots[LabSlot].Slot; }
		T = LabFrame; Raw = LabFrame; LabMove = MoveClip.Find(Clip); MoveId = LabMove;
	}
	auto Ease = [](float K) { K = FMath::Clamp(K, 0.f, 1.f); return K * K * (3.f - 2.f * K); };
	if (!bLab) switch (P.st)
	{
		case gk::IDLE:
			if (P.sub == gk::CROUCH) { Clip = TEXT("crouch"); T = FMath::Min(P.subf, 16); Raw = P.subf; }
			else if (P.sub == gk::WALKF) { Clip = TEXT("walkf"); T = P.subf < 65 ? P.subf : 5 + ((P.subf - 5) % 60); }
			else if (P.sub == gk::WALKB) { Clip = TEXT("walkb"); T = P.subf < 71 ? P.subf : 5 + ((P.subf - 5) % 66); }
			break;
		case gk::PREJUMP: Clip = TEXT("prejump"); T = P.sf; break;
		case gk::LAND: Clip = TEXT("land"); T = P.sf; break;
		case gk::JUMP:
		{
			// a tight tuck going up, opening out on the way down; a jump with any travel in it is a somersault
			const float U = FMath::Clamp((1125.f - P.vy) / 2250.f, 0.f, 1.f); Clip = TEXT("jump"); T = FMath::RoundToInt(U * 20.f);
			if (P.jd != 0) { Roll = P.jd * 360.f * Ease(FMath::Min(1.f, U / 0.8f)); }
			break;
		}
		case gk::TECH: Clip = TEXT("tech"); T = P.sf; break;
		case gk::DASHF: Clip = TEXT("dashf"); T = P.sf; break;
		case gk::DASHB: Clip = TEXT("dashb"); T = P.sf; break;
		case gk::RUN: Clip = TEXT("run"); T = P.sf; break;
		case gk::DRUSH: Clip = TEXT("drush"); T = P.sf; break;
		case gk::PARRY: Clip = TEXT("parry"); T = P.sf; break;
		case gk::PREC: Clip = TEXT("prec"); T = P.sf; break;
		case gk::BLOCK: Clip = P.cb ? TEXT("blockC") : TEXT("blockS"); T = 0; break;
		case gk::HIT:
			if (P.crum == 1) { Clip = TEXT("crumple"); T = P.sf; }
			else if (P.crum == 2) { Clip = TEXT("splat"); T = 0; }
			else
			{
				static const TCHAR* Kinds[4] = { TEXT("hitL"), TEXT("hitM"), TEXT("hitH"), TEXT("hitLow") };
				Clip = P.cb ? TEXT("hitCr") : Kinds[FMath::Clamp(HitKind[i], 0, 3)]; T = FMath::RoundToInt(20.f * P.sf / FMath::Max(1, P.sf + P.stun));
			}
			break;
		// knocked into the air he goes limp and turns over like a thrown coat
		case gk::AHIT: Clip = TEXT("ahit"); T = 0; Roll = -30.f - FMath::Clamp((700.f - P.vy) / 16.f, 0.f, 80.f); Turn = P.sf * 7.f; break;
		case gk::KDOWN: Clip = TEXT("kd"); T = 0; break;
		case gk::GETUP_: Clip = TEXT("getup"); T = P.sf; break;
		case gk::WIN: Clip = TEXT("win"); T = P.sf; break;
		case gk::THROWING:
			if (P.tk == 0) { if (P.sf < 10) { Clip = TEXT("grab"); T = P.sf; } else if (P.pmT > 0) { Clip = TEXT("bite"); T = 9 - P.pmT; } else { Clip = TEXT("grab"); T = 10; } }
			else { static const TCHAR* Throws[4] = { TEXT("throw1"), TEXT("throw2"), TEXT("throw3"), TEXT("throw4") }; Clip = Throws[FMath::Clamp(P.tk, 1, 4) - 1]; T = P.tf; }
			break;
		case gk::THROWN: Clip = TEXT("thrown"); T = 0; Roll = O.tk ? -FMath::Min(120.f, O.tf * 12.f) : -8.f; break;
		case gk::ATK:
		{
			const gk::Move& M = gk::move(P.mv);
			if (MoveClip.IsValidIndex(P.mv)) { Clip = MoveClip[P.mv]; T = P.mf; MoveId = P.mv; }
			// a Slipstream that did not land cleanly loses its shape and tumbles
			if (M.has_home && P.mf >= M.home[0] && ((P.contact && !P.fr) || P.hmE)) { Clip = TEXT("ahit"); T = 0; Roll = -60.f - 30.f * FMath::Sin(P.mf * 0.5f); MoveId = -1; }
			break;
		}
		default: break;
	}
	const bool bLeft = !bLab && P.face < 0;
	FVector Jit = FVector::ZeroVector;
	if (P.hs > 0 && (P.st == gk::HIT || P.st == gk::AHIT || P.st == gk::BLOCK)) { Jit.Y = ((Ticks & 1) ? 1.0 : -1.0) * FMath::Min(4.0, P.hs * 0.6); }
	// The skeleton faces its own +Y, which is the rules' "right"; the camera looks along +X. Facing left he is the mirrored
	// pose turned about, which shows the camera his chest exactly as the browser's mirror image does.
	FVector Where = bLab ? Wd(FVector(0.0, (bScript && !bLabUi) ? 0.0 : 62.0 * Cm * gk::F, 0.0)) : Wd(At(P.x, P.y, i ? 8.0 : -8.0) + Jit); const FQuat Facing = Anchor.GetRotation() * FRotator(0.0, bLeft ? 180.0 : 0.0, 0.0).Quaternion();
	if (bForestLook && bPlaced && !bLab)
	{
		const FVector Local = At(P.x, 0, i ? 8.0 : -8.0);
		Where.Z += Wd(FloorPoint(Local)).Z - Wd(Local).Z;
	}
	const bool bTry = bLab && bLabDrop && LabTryAt >= -1;
	const FGkClip* Use = bTry ? (LabTry.Anim ? &LabTry : nullptr) : (ClipSet[i] >= 0 ? ClipFor(ClipSet[i], Clip, MoveId) : nullptr);
	if (Anim[i]) { Anim[i]->SetVisibility(false); } C->SetVisibility(true);
	USkinnedAsset* SA = C->GetSkinnedAsset(); if (!SA) { return; }
	const FReferenceSkeleton& RS = SA->GetRefSkeleton(); const int32 N = RS.GetNum(); const TArray<FTransform>& RefLocal = RS.GetRefBonePose();
	if (C->BoneSpaceTransforms.Num() != N || N < 2) { const float* Pose0 = Poses.Frame(Clip, T); Rig[i].Apply(Pose0 ? Pose0 : Poses.Frame(TEXT("idle"), 0), !bLab && P.y > 0, bLeft, Roll, Turn); C->SetWorldLocationAndRotation(Where, Facing); return; }
	if (PoseMesh[i] != SA)
	{
		// a new body: its standing pose in the mesh's own space, which bone is whose opposite number, and the bones the timing reads
		PoseMesh[i] = SA; PoseShown[i].Reset(); ShownAnim[i] = nullptr; ShownClip[i] = NAME_None; SkelFor[i] = nullptr;
		RefCS[i].SetNum(N); Twin[i].SetNum(N);
		for (int32 b = 0; b < N; b++)
		{
			const int32 Pa = RS.GetParentIndex(b); RefCS[i][b] = Pa >= 0 ? RefLocal[b] * RefCS[i][Pa] : RefLocal[b];
			FString Nm = RS.GetBoneName(b).ToString(); Twin[i][b] = b;
			if (Nm.EndsWith(TEXT("_l"))) { const int32 O2 = RS.FindBoneIndex(FName(*(Nm.LeftChop(2) + TEXT("_r")))); if (O2 != INDEX_NONE) { Twin[i][b] = O2; } }
			else if (Nm.EndsWith(TEXT("_r"))) { const int32 O2 = RS.FindBoneIndex(FName(*(Nm.LeftChop(2) + TEXT("_l")))); if (O2 != INDEX_NONE) { Twin[i][b] = O2; } }
		}
		static const TCHAR* Keys[5] = { TEXT("pelvis"), TEXT("hand_l"), TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r") };
		for (int32 k = 0; k < 5; k++) { KeyBone[i][k] = RS.FindBoneIndex(FName(Keys[k])); }
	}
	static TArray<FTransform> Tg, Cs, Ms; FVector Wh = Where; FQuat Fq = Facing;
	if (Use)
	{
		const float Len = Use->Anim->GetPlayLength(), A0 = Use->From * Len, A1 = FMath::Max(Use->To * Len, A0 + 0.01f); float Sec = A0; const int32 Count = FMath::Max(2, Poses.Length(Clip));
		const FString Cn = Clip.ToString();
		if (bTry || (bLab && (!bScript || bLabUi) && (Use->bLoop || Poses.Length(Clip) <= 2))) { const float Span = A1 - A0; Sec = A0 + FMath::Min(Span, FMath::Fmod(LabClock, Span + 0.4f)); }
		else if (Use->bLoop) { const float Span = FMath::Max(0.05f, A1 - A0); Sec = A0 + FMath::Fmod((bLab ? LabFrame * 4 : S.frame + i * 17) / 60.f, Span); }
		else if (MoveId >= 0 && (!gk::move(MoveId).sup || MoveId == gk::moveByKey("SA3")))
		{
			// one blow: wind up to the clip's furthest reach on the first active frame, hold through the active frames, then recover
			const gk::Move& M = gk::move(MoveId); const bool bMany = M.nhits > 1; const int32 Su = FMath::Max(1, M.has_proj ? M.pf : bMany ? M.hits[0].s : M.su), Ac = FMath::Max(1, bMany ? M.hits[M.nhits - 1].e - M.hits[0].s + 1 : M.ac), Tot = M.t < 900 ? FMath::Max(M.t, Su + Ac + 1) : Su + Ac + FMath::Max(M.rc, 20), Rc = FMath::Max(1, Tot - Su - Ac);
			const float Th = FMath::Clamp(ImpactOf(i, *Use, M.has_proj || M.sup) * Len, A0, A1), T0 = FMath::Max(A0, Th - Su / 60.f * 2.5f), Hold = Ac / 60.f * 0.4f, T1 = FMath::Max(Th + Hold + 0.01f, FMath::Min(A1, Th + Hold + Rc / 60.f * 1.8f));
			// (the blow is fully out a frame before the first active one, so the frame it lands on, and is held on, shows it at full reach)
			if (T < Su - 1) { Sec = T0 + (Th - T0) * FMath::Pow(float(T) / float(FMath::Max(1, Su - 1)), 1.5f); }
			else if (T < Su + Ac) { Sec = Th + FMath::Max(0, T - Su) / 60.f * 0.4f; }
			else { const float u = FMath::Clamp(float(T - Su - Ac) / float(Rc), 0.f, 1.f); Sec = FMath::Lerp(Th + Hold, T1, u * (2.f - u)); }
		}
		else if (MoveId >= 0) { const gk::Move& M = gk::move(MoveId); const int32 Tot = M.t < 900 ? FMath::Max(2, M.t) : Count; Sec = FMath::Lerp(A0, A1, FMath::Clamp(float(T) / float(Tot), 0.f, 1.f)); }
		else if (Cn.StartsWith(TEXT("hit")) || Cn == TEXT("crumple") || Cn == TEXT("crouch") || Cn == TEXT("blockS") || Cn == TEXT("ahit") || Cn == TEXT("kd")) { Sec = FMath::Min(A1, A0 + Raw / 60.f * 1.25f); }
		else { Sec = FMath::Lerp(A0, A1, FMath::Clamp(float(T) / float(Count - 1), 0.f, 1.f)); }
		EvalClip(i, Use->Anim, FMath::Clamp(Sec, 0.f, Len), Tg);
		// the rules own where he stands: the hips may shift their weight but not walk off
		if (KeyBone[i][0] > 0)
		{
			const FVector R0 = RefLocal[KeyBone[i][0]].GetTranslation(); FVector Dl = Tg[KeyBone[i][0]].GetTranslation() - R0;
			Dl.X = 18.0 * FMath::Tanh(Dl.X / 18.0); Dl.Y = 35.0 * FMath::Tanh(Dl.Y / 35.0); if (Dl.Z > 0.0) { Dl.Z = 14.0 * FMath::Tanh(Dl.Z / 14.0); } Tg[KeyBone[i][0]].SetTranslation(R0 + Dl);
		}
		if (bLeft)
		{
			// the mirror image: every bone takes its opposite number's place, reflected through the body's own middle
			Cs.SetNum(N); Ms.SetNum(N);
			for (int32 b = 0; b < N; b++) { const int32 Pa = RS.GetParentIndex(b); Cs[b] = Pa >= 0 ? Tg[b] * Cs[Pa] : Tg[b]; }
			auto MirQ = [](const FQuat& Q) { return FQuat(Q.X, -Q.Y, -Q.Z, Q.W); };
			for (int32 b = 0; b < N; b++)
			{
				const int32 Tw = Twin[i][b]; const FVector L = Cs[Tw].GetTranslation();
				FQuat Q = MirQ(Cs[Tw].GetRotation()) * MirQ(RefCS[i][Tw].GetRotation()).Inverse() * RefCS[i][b].GetRotation(); Q.Normalize();
				Ms[b] = FTransform(Q, FVector(-L.X, L.Y, L.Z), Cs[Tw].GetScale3D());
			}
			for (int32 b = 0; b < N; b++) { const int32 Pa = RS.GetParentIndex(b); Tg[b] = Pa >= 0 ? Ms[b].GetRelativeTransform(Ms[Pa]) : Ms[b]; }
		}
		if ((Roll != 0.f || Turn != 0.f) && (bLab || P.st != gk::JUMP) && Cn != TEXT("ahit"))
		{
			// a tumble turns the whole body about its hips
			const FQuat Tm = FQuat(FVector(0, 0, 1), FMath::DegreesToRadians(Turn)) * FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(-Roll)); const FVector Piv(0.0, 0.0, 95.0);
			Fq = Facing * Tm; Wh = Where + Facing.RotateVector(Piv) - Fq.RotateVector(Piv);
		}
	}
	else
	{
		C->BoneSpaceTransforms = RefLocal;
		const float OwnT = Clip == FName(TEXT("thrown")) ? (bLab ? float(LabFrame % 20) : float(S.p[1 - i].tk ? 8 : FMath::Min(P.sf, 4))) : Clip == FName(TEXT("splat")) ? float(bLab ? LabFrame : Raw) : float(T);
		const float* Pose = GkOwnPose(Clip, OwnT); if (!Pose) { Pose = Poses.Frame(Clip, T); } if (!Pose) { Pose = Poses.Frame(TEXT("idle"), 0); }
		Rig[i].Apply(Pose, !bLab && P.y > 0, bLeft, Roll, Turn); Tg = C->BoneSpaceTransforms;
	}
	// the change from one clip to the next is a short cross-fade from whatever was showing: quick into a blow or a reel, easy back to standing
	const void* NowAnim = Use ? (const void*)Use->Anim.Get() : nullptr;
	const bool bAgain = MoveId >= 0 && T < LastT[i] - 1; LastT[i] = T;
	if (NowAnim != ShownAnim[i] || (!Use && ShownClip[i] != Clip) || bAgain || PoseShown[i].Num() != N)
	{
		const bool bFirst = PoseShown[i].Num() != N; PoseFrom[i] = bFirst ? Tg : PoseShown[i]; BlendAt[i] = AnimClock; ShownAnim[i] = NowAnim; ShownClip[i] = Clip;
		const FString Cn = Clip.ToString(); const bool bSharp = !bLab && (P.st == gk::HIT || P.st == gk::AHIT || P.st == gk::BLOCK || P.st == gk::PARRY || P.st == gk::PREC || P.st == gk::THROWN || P.st == gk::KDOWN);
		BlendLen[i] = (bLab || bFirst) ? 0 : bSharp ? 2 : MoveId >= 0 ? 3 : (Cn == TEXT("idle") ? 9 : Cn.StartsWith(TEXT("walk")) ? 7 : 5);
	}
	float Al = BlendLen[i] > 0 ? FMath::Clamp(float(AnimClock - BlendAt[i] + 1) / float(BlendLen[i]), 0.f, 1.f) : 1.f; Al = Al * Al * (3.f - 2.f * Al);
	PoseShown[i].SetNum(N);
	if (Al >= 1.f || PoseFrom[i].Num() != N) { PoseShown[i] = Tg; }
	else { for (int32 b = 0; b < N; b++) { FTransform Bl; Bl.Blend(PoseFrom[i][b], Tg[b], Al); PoseShown[i][b] = Bl; } }
	C->BoneSpaceTransforms = PoseShown[i]; C->MarkRefreshTransformDirty();
	// a blow rocks him back on his heels; a landing squashes him for an instant
	if (!bLab && FMath::Abs(Lean[i]) > 0.3f) { Fq = Fq * FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(Lean[i])); }
	const float Sq = bLab ? 0.f : Squash[i]; C->SetWorldScale3D(FVector(1.0 + 0.07 * Sq, 1.0 + 0.07 * Sq, 1.0 - 0.12 * Sq));
	C->SetWorldLocationAndRotation(Wh, Fq);
	LookFighter(i);
	Afterimage(i);
}

/** The copies a fast-moving fighter leaves behind him. They cannot be seen through, so each one flickers out instead. */
void AGkMatch::Afterimage(int32 i)
{
	UPoseableMeshComponent* C = Body[i]; if (!C) { return; }
	const gk::Fighter& P = S.p[i]; const gk::Move* M = (P.st == gk::ATK && P.mv >= 0) ? &gk::move(P.mv) : nullptr;
	const bool bOn = Screen == 3 && !bMenu && BodyMid[i][0] != nullptr && C->GetSkinnedAsset() != nullptr;
	const bool bFast = bOn && S.freeze <= 0 && (P.st == gk::DRUSH || P.st == gk::RUN || P.st == gk::DASHF || P.st == gk::DASHB || (M && (M->di || (M->sup && P.mf > 6) || (M->has_home && P.mf >= M->home[0] && !P.hmE))) || (P.st == gk::AHIT && FMath::Abs(P.vx) > 260));
	const bool bStep = AnimClock != GhostClock[i]; GhostClock[i] = AnimClock;
	if (bStep) { for (int32 g = 0; g < 3; g++) { GhostAge[i * 3 + g]++; } }
	if (bStep && bFast && (AnimClock % 3) == 0)
	{
		const int32 g = i * 3 + (GhostNext[i]++ % 3);
		if (!Ghost[g])
		{
			UPoseableMeshComponent* Gc = NewObject<UPoseableMeshComponent>(this); Gc->SetMobility(EComponentMobility::Movable); Gc->SetupAttachment(RootComponent);
			Gc->SetUsingAbsoluteLocation(true); Gc->SetUsingAbsoluteRotation(true); Gc->SetUsingAbsoluteScale(true); Gc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Gc->SetCastShadow(false);
			Gc->RegisterComponent(); Gc->SetBoundsScale(30.f); Gc->bNeverDistanceCull = true; Ghost[g] = Gc;
		}
		UPoseableMeshComponent* Gc = Ghost[g];
		if (Gc->GetSkinnedAsset() != C->GetSkinnedAsset())
		{
			Gc->EmptyOverrideMaterials(); Gc->SetSkinnedAssetAndUpdate(C->GetSkinnedAsset());
			GhostMid[g] = UMaterialInstanceDynamic::Create(BodyMid[i][0]->Parent, this); for (int32 k = 0; k < Gc->GetNumMaterials(); k++) { Gc->SetMaterial(k, GhostMid[g]); }
		}
		if (Gc->BoneSpaceTransforms.Num() == C->BoneSpaceTransforms.Num())
		{
			Gc->BoneSpaceTransforms = C->BoneSpaceTransforms; Gc->MarkRefreshTransformDirty(); Gc->SetWorldTransform(C->GetComponentTransform()); GhostAge[g] = 0;
			const FLinearColor Tint = (M && M->di) ? FLinearColor(6.f, 0.5f, 3.6f) : P.st == gk::DRUSH ? FLinearColor(0.4f, 4.5f, 1.4f) : P.st == gk::AHIT ? FLinearColor(3.2f, 3.2f, 3.6f) : i ? FLinearColor(0.5f, 1.6f, 5.f) : FLinearColor(5.f, 2.2f, 0.4f);
			if (GhostMid[g]) { GhostMid[g]->SetVectorParameterValue(TEXT("Base  color"), Tint); GhostMid[g]->SetVectorParameterValue(TEXT("Shadow_Base"), Tint * 0.7f); GhostMid[g]->SetVectorParameterValue(TEXT("Rimlight Color"), FLinearColor(6.f, 6.f, 6.f)); }
		}
	}
	for (int32 g = i * 3; g < i * 3 + 3; g++) { if (Ghost[g]) { Ghost[g]->SetVisibility(bOn && (GhostAge[g] < 2 || (GhostAge[g] < 9 && ((GhostAge[g] + g) & 1) == 0))); } }
}

/** The fighter's colours and outline for the state he is in. */
void AGkMatch::LookFighter(int32 i)
{
	if (!InkMid[i]) { return; }
	const gk::Fighter& P = S.p[i]; const bool bLab = Screen == 4; const gk::Move* M = (!bLab && P.st == gk::ATK && P.mv >= 0) ? &gk::move(P.mv) : nullptr;
	FLinearColor Edge(0.01f, 0.01f, 0.015f), Wash(0.f, 0.f, 0.f); float Thick = bForestLook ? 0.58f : 1.f, Amt = 0.f; const float Pulse = 0.5f + 0.5f * FMath::Sin(Ticks * 0.55f);
	const FLinearColor Ki = i ? FLinearColor(0.5f, 1.6f, 5.f) : FLinearColor(5.f, 2.2f, 0.4f);
	if (!bLab)
	{
		if (P.bo) { Wash = FLinearColor(0.3f, 0.33f, 0.31f); Amt = 0.62f + 0.12f * Pulse; Edge = FLinearColor(0.2f, 0.26f, 0.22f); Thick = 1.5f; }
		if (M && M->sup) { Edge = FLinearColor(9.f, 6.5f, 1.2f); Thick = 3.f + Pulse; Wash = FLinearColor(3.f, 2.2f, 0.6f); Amt = 0.3f; }
		else if (M && M->od) { Edge = FLinearColor(7.f, 4.f, 0.5f); Thick = 2.6f; Wash = FLinearColor(2.f, 1.2f, 0.3f); Amt = 0.18f; }
		else if (M && M->sp) { Edge = Ki; Thick = 2.f; }
		else if (M && M->di) { const float Fl = (Ticks & 2) ? 1.f : 0.6f; Edge = FLinearColor(10.f, 0.8f, 6.f) * Fl; Thick = 4.5f; Wash = FLinearColor(5.f, 0.5f, 3.2f); Amt = 0.45f * Fl; }
		if (P.arm > 0) { Edge = FLinearColor(6.f, 1.8f, 0.2f); Thick = 2.8f; }
		if (P.st == gk::DRUSH) { Edge = FLinearColor(0.4f, 6.f, 1.4f); Thick = 2.6f; Wash = FLinearColor(0.3f, 2.4f, 0.8f); Amt = 0.3f; }
		if (P.st == gk::BLOCK) { Edge = FLinearColor(2.f, 4.f, 9.f); Thick = 3.2f; Wash = FLinearColor(1.1f, 1.6f, 3.2f); Amt = P.hs > 0 ? 0.75f : 0.4f; }
		if (P.st == gk::PARRY) { Edge = FLinearColor(0.2f, 6.f, 7.f) * (0.7f + 0.6f * Pulse); Thick = 3.f + 1.5f * Pulse; Wash = FLinearColor(0.3f, 2.4f, 2.8f); Amt = 0.45f; }
		if (P.st == gk::PREC) { Edge = FLinearColor(0.2f, 0.45f, 0.5f); Thick = 1.3f; Wash = FLinearColor(0.16f, 0.3f, 0.34f); Amt = 0.5f; }
		if (PerfectT[i] > 0) { const float K = PerfectT[i] / 50.f; Edge = FMath::Lerp(Edge, FLinearColor(10.f, 7.f, 1.5f), K); Thick = FMath::Max(Thick, 4.f * K); Wash = FMath::Lerp(Wash, FLinearColor(6.f, 4.5f, 1.5f), K); Amt = FMath::Max(Amt, 0.8f * K); }
		if (P.st == gk::HIT || P.st == gk::AHIT || P.st == gk::THROWN)
		{
			if (P.hs > 0) { Wash = FLinearColor(4.f, 4.f, 4.f); Amt = 0.85f; Edge = FLinearColor(7.f, 1.f, 0.5f); Thick = 2.6f; }
			else { Wash = FMath::Lerp(Wash, FLinearColor(1.8f, 0.12f, 0.08f), 0.5f); Amt = FMath::Max(Amt, 0.24f); Edge = FLinearColor(1.6f, 0.1f, 0.05f); Thick = 1.6f; }
		}
	}
	// lightning behind them: for a few frames they are black shapes cut out of the white sky
	if (!bLab && BoltT > 0.f) { const float K = FMath::Clamp(BoltT / 4.f, 0.f, 1.f); Wash = FMath::Lerp(Wash, FLinearColor(0.f, 0.f, 0.01f), K); Amt = FMath::Lerp(Amt, bForestLook ? 0.28f : 0.96f, K); Edge = FMath::Lerp(Edge, bForestLook ? FLinearColor(0.27f, 0.20f, 0.55f) : FLinearColor(9.f, 10.f, 13.f), K); Thick = FMath::Max(Thick, (bForestLook ? 0.78f : 2.6f) * K); }
	InkMid[i]->SetVectorParameterValue(TEXT("Outlines Color"), Edge); InkMid[i]->SetScalarParameterValue(TEXT("Outlines_Thickness"), Thick);
	// the rim: the colour of the lamp behind them, brighter in a storm's flashes and dimmed with the world under a super
	const bool bRim = Kind != 0 || bLab; const FLinearColor Rim = (bForestLook ? ForestRim : RimCol) * ((bRim ? 1.f : 0.35f) * (1.f + 3.f * FMath::Min(Flash, 1.f)) * (1.f + 0.6f * Storm));
	float SceneDir = 0.f; const FLinearColor SceneT = GkSceneTint(GetWorld(), SceneDir); const FLinearColor SceneS = FLinearColor(0.92f, 0.95f, 1.f);
	for (int32 k = 0; k < 4; k++)
	{
		if (!BodyMid[i][k]) { continue; }
		if (ForestFighterCel && BodyMid[i][k]->Parent == ForestFighterCel)
		{
			const bool bArmor = k == 0;
			const FLinearColor Paint = bArmor
				? (i ? FLinearColor(0.085f, 0.20f, 0.43f) : FLinearColor(0.47f, 0.065f, 0.06f))
				: FLinearColor(0.65f, 0.65f, 0.59f);
			const FLinearColor Shadow = bArmor
				? (i ? FLinearColor(0.018f, 0.040f, 0.13f) : FLinearColor(0.13f, 0.018f, 0.031f))
				: FLinearColor(0.17f, 0.19f, 0.24f);
			const FVector Key = ForestSun.IsValid() ? -ForestSun->GetForwardVector() : FVector(-0.4, -0.5, 0.75);
			const FLinearColor Tint = FMath::Lerp(FLinearColor::White, SceneT, 0.38f);
			BodyMid[i][k]->SetVectorParameterValue(TEXT("Key"), FLinearColor(Key.X, Key.Y, Key.Z));
			BodyMid[i][k]->SetVectorParameterValue(TEXT("KeyTint"), Tint);
			BodyMid[i][k]->SetVectorParameterValue(TEXT("Paint"), FMath::Lerp(Paint, Wash, Amt));
			BodyMid[i][k]->SetVectorParameterValue(TEXT("Shadow"), FMath::Lerp(Shadow, Wash * 0.55f, Amt));
			BodyMid[i][k]->SetVectorParameterValue(TEXT("Rim"), Rim * 0.52f);
			continue;
		}
		BodyMid[i][k]->SetVectorParameterValue(TEXT("Rimlight Color"), Rim); BodyMid[i][k]->SetScalarParameterValue(TEXT("Rimlight Thickness"), RimTune[0]); BodyMid[i][k]->SetScalarParameterValue(TEXT("Rimlight Opacity"), RimTune[1]); BodyMid[i][k]->SetScalarParameterValue(TEXT("Rim Brightness"), RimTune[2]);
		BodyMid[i][k]->SetVectorParameterValue(TEXT("Base  color"), FMath::Lerp(BaseCol[i][k] * SceneT, Wash, Amt)); BodyMid[i][k]->SetVectorParameterValue(TEXT("Shadow_Base"), FMath::Lerp(ShadeCol[i][k] * SceneT * SceneS, Wash * 0.55f, Amt));
	}
}

/** A beam of light along the stage from a point (in the fight's own space): a white-hot core inside a glow of its owner's colour. */
void AGkMatch::Beam(const FVector& From, float Dir, float Len, float R, const FLinearColor& Col)
{
	if (Len < 8.f) { return; }
	const int32 Count = FMath::Clamp(int32(Len / (R * 0.6f)) + 1, 2, 26); const float Step = Len / float(Count - 1), Wob = 1.f + 0.12f * FMath::Sin(Ticks * 1.3f);
	for (int32 k = 0; k < Count; k++)
	{
		const FVector At2 = From + FVector(0.0, Dir * Step * k, Fx.Rng.FRandRange(-0.05f, 0.05f) * R);
		Fx.Add(0, At2, FVector::ZeroVector, 0.04f, R * 2.3f * Wob, R * 2.f, Col * 0.3f); Fx.Add(0, At2 + FVector(-4.0, 0.0, 0.0), FVector::ZeroVector, 0.04f, R * 1.05f * Wob, R * 0.9f, FLinearColor(5.f, 5.f, 4.6f));
	}
	// long streaks laid end over end make it one unbroken shaft
	const float Seg = FMath::Max(260.f, R * 4.f); const int32 Segs = FMath::Clamp(int32(Len / (Seg * 0.5f)), 1, 14);
	for (int32 k = 0; k < Segs; k++)
	{
		const float Mid = FMath::Min(Len - Seg * 0.5f, Seg * 0.5f + k * (Len - Seg) / float(FMath::Max(1, Segs - 1))); if (Len < Seg) { break; }
		const FVector At2 = From + FVector(0.0, Dir * Mid, 0.0);
		FGkFx::FP& Sh = Fx.Add(0, At2 + FVector(2.0, 0.0, 0.0), FVector(0.0, Dir * 600.0, 0.0), 0.04f, R * 3.2f * Wob, R * 3.f, Col * 0.45f); Sh.Stretch = Seg / (R * 3.2f);
		FGkFx::FP& Co = Fx.Add(0, At2 + FVector(-6.0, 0.0, 0.0), FVector(0.0, Dir * 600.0, 0.0), 0.04f, R * 1.7f * Wob, R * 1.6f, FLinearColor(6.f, 6.f, 5.6f)); Co.Stretch = Seg / (R * 1.7f);
	}
	// the mouth of it flares, and sparks tear off along its length
	Fx.Add(0, From, FVector::ZeroVector, 0.05f, R * 3.4f * Wob, R * 3.f, Col * 0.4f); Fx.Add(0, From, FVector::ZeroVector, 0.05f, R * 1.7f, R * 1.4f, FLinearColor(6.f, 6.f, 5.5f));
	for (int32 k = 0; k < 2; k++) { FGkFx::FP& Sp = Fx.Add(0, From + FVector(0.0, Dir * Fx.Rng.FRandRange(0.f, Len), Fx.Rng.FRandRange(-R, R)), FVector(0.0, Dir * Fx.Rng.FRandRange(500.f, 1400.f), Fx.Rng.FRandRange(-260.f, 260.f)), 0.25f, R * 0.14f, 2.f, Col * 1.4f); Sp.Stretch = 9.f; }
}

/** Particles that go with a state, once a step: the parry's ring, burnout's smoke, the charge of a special, the wake of a rush. */
void AGkMatch::StateFx(int32 i)
{
	const gk::Fighter& P = S.p[i]; const FVector Chest = At(P.x, P.y) + FVector(-20.0, 0.0, 95.0); const gk::Move* M = (P.st == gk::ATK && P.mv >= 0) ? &gk::move(P.mv) : nullptr;
	const FLinearColor Ki = i ? FLinearColor(1.5f, 3.5f, 8.f) : FLinearColor(7.f, 3.6f, 0.9f);
	if ((Kind == 1 || (bForestLook && bPlaced)) && P.y <= 0 && P.vx != 0)
	{
		const bool bDash = P.st == gk::DASHF || P.st == gk::DASHB || P.st == gk::DRUSH;
		const bool bMoving = bDash || P.st == gk::RUN || (P.st == gk::IDLE && P.sub != gk::CROUCH);
		if (bMoving && (Ticks % (bDash ? 4 : 10)) == i)
		{
			const FVector Feet = bForestLook ? FloorPoint(At(P.x, 0)) : At(P.x, 0);
			if (bForestLook)
			{
				if ((Ticks % (bDash ? 8 : 20)) == i)
				{
					const float Direction = P.vx > 0 ? 1.f : -1.f;
					if (!PlayVfx(bDash ? TEXT("NS_Directional_Dust_03_Once_Fast") : TEXT("NS_Directional_Dust_01_Once"),
						Feet, bDash ? 0.48f : 0.33f, -Direction, nullptr))
					{
						Fx.Dust(Feet, bDash ? 0.65f : 0.30f, Direction);
					}
					if (bDash)
					{
						FGkFx::FP& Chip = Fx.Add(1, Feet + FVector(-8.0, 0.0, 8.0),
							FVector(0.0, -140.0 * Direction, 120.0), 0.26f, 5.f, 3.f, Fx.DustCol);
						Chip.Grav = 760.f;
					}
				}
			}
			else { Fx.CanyonFootDust(Feet, P.vx > 0 ? 1.0 : -1.0, bDash); }
		}
	}
	// his feet: a heavy step as he walks, quicker when he runs
	{
		const bool bWalk = P.st == gk::IDLE && P.sub != gk::CROUCH && P.y <= 0 && FMath::Abs(P.vx) > 20, bRun = (P.st == gk::RUN || P.st == gk::DRUSH) && P.y <= 0;
		if (bWalk || bRun)
		{
			if (++StepT[i] >= (bRun ? 13 : 24))
			{
				StepT[i] = 0;
				Snd(TEXT("Step_01"), bRun ? 0.3f : 0.22f, bRun ? 1.15f : 1.f);
				if (ShrineWater)
				{
					const FVector Foot = At(P.x, 0) + FVector(0.0, 0.0, 4.0);
					const FLinearColor WaterTint(1.3f, 1.7f, 2.1f);
					for (int32 Side : {-1, 1})
					{
						FGkFx::FP& Drop = Fx.Add(0, Foot, FVector(0.0, Side * 65.0, bRun ? 155.0 : 110.0), 0.22f, 4.f, 1.5f, WaterTint);
						Drop.Grav = 800.f;
						Drop.Stretch = 2.f;
					}
				}
			}
		}
		else { StepT[i] = 16; }
	}
	if (P.st == gk::PARRY && (Ticks % 3) == 0)
	{
		for (int32 k = 0; k < 8; k++)
		{
			const float A = (k + (Ticks % 6) * 0.17f) * 2.f * PI / 8.f; const FVector Out(0.0, FMath::Cos(A), FMath::Sin(A));
			FGkFx::FP& R = Fx.Add(0, Chest + Out * 95.0, Out * -160.0, 0.22f, 11.f, 3.f, FLinearColor(0.6f, 6.f, 7.f)); R.Stretch = 5.f;
		}
		Fx.Add(0, Chest, FVector::ZeroVector, 0.1f, 210.f, 240.f, FLinearColor(0.1f, 1.1f, 1.3f));
	}
	if (P.bo && (Ticks % 7) == i * 3)
	{
		FGkFx::FP& Sm = Fx.Add(1, Chest + FVector(Fx.Rng.FRandRange(-10.f, 10.f), Fx.Rng.FRandRange(-25.f, 25.f), Fx.Rng.FRandRange(-40.f, 60.f)), FVector(0.0, Fx.Rng.FRandRange(-20.f, 20.f), Fx.Rng.FRandRange(60.f, 130.f)), 0.9f, 16.f, 52.f, FLinearColor(0.05f, 0.06f, 0.055f), 0.38f);
		Sm.Drag = 1.2f;
	}
	if (M && M->di)
	{   // all the way through a drive impact he drags bright streaks behind him
		static const FLinearColor Paint[3] = { FLinearColor(9.f, 0.6f, 5.f), FLinearColor(0.5f, 6.f, 8.f), FLinearColor(9.f, 6.f, 0.5f) };
		for (int32 k = 0; k < 3; k++) { FGkFx::FP& St = Fx.Add(0, Chest + FVector(0.0, -P.face * 20.0, Fx.Rng.FRandRange(-80.f, 70.f)), FVector(0.0, -P.face * Fx.Rng.FRandRange(500.f, 1100.f), Fx.Rng.FRandRange(-60.f, 60.f)), 0.3f, Fx.Rng.FRandRange(12.f, 24.f), 3.f, Paint[(Ticks + k) % 3]); St.Stretch = 12.f; }
		Fx.Add(0, Chest, FVector::ZeroVector, 0.1f, 170.f, 210.f, FLinearColor(1.6f, 0.15f, 1.f));
	}
	if (M && P.mv == gk::moveByKey("SA3"))
	{   // King Tide: light gathers between the hands, then the wave runs the length of the stage
		const int32 Fire = M->hits[0].s - 2, End = M->lastAct + 3; const FVector Hands = At(P.x + P.face * 34 * gk::F, P.y + 56 * gk::F, -10.0);
		if (P.mf < Fire) { const float Ch = FMath::Clamp(P.mf / float(FMath::Max(1, Fire)), 0.f, 1.f); Fx.Add(0, Hands, FVector::ZeroVector, 0.05f, 60.f + 150.f * Ch, 50.f + 130.f * Ch, Ki * 0.5f); Fx.Add(0, Hands, FVector::ZeroVector, 0.05f, 24.f + 70.f * Ch, 20.f + 60.f * Ch, FLinearColor(6.f, 6.f, 5.5f));
			for (int32 k = 0; k < 2; k++) { const float A = Fx.Rng.FRandRange(0.f, 2.f * PI); const FVector Out(0.0, FMath::Cos(A), FMath::Sin(A)); FGkFx::FP& In = Fx.Add(0, Hands + Out * 170.0, Out * -520.0, 0.3f, 9.f, 2.f, Ki); In.Stretch = 7.f; } }
		else if (P.mf <= End)
		{
			const float Grow = FMath::Clamp((P.mf - Fire + 1) / 4.f, 0.f, 1.f), Fade = FMath::Clamp((End - P.mf + 1) / 5.f, 0.f, 1.f);
			if (P.mf == Fire) { Snd(TEXT("AbilityRelease_05"), 0.9f); Snd(TEXT("LaserBig_01"), 1.f); Snd(TEXT("Explosion_01"), 0.6f); }
			Beam(Hands, float(P.face), 2600.f * Grow, 150.f * FMath::Min(Grow, Fade), Ki); Shake = FMath::Max(Shake, 5.f); Flash = FMath::Max(Flash, P.mf == Fire ? 0.8f : 0.1f);
			if ((Ticks % 5) == 0) { Fx.Dust(At(P.x + P.face * Fx.Rng.FRandRange(60.f, 500.f) * gk::F, 0), 1.2f, float(P.face)); }
		}
	}
	if (M && (M->sup || M->sp) && (Ticks % 2) == 0)
	{
		Fx.Add(0, Chest, FVector::ZeroVector, 0.12f, M->sup ? 120.f : 80.f, M->sup ? 200.f : 130.f, Ki * (M->sup ? 0.1f : 0.05f));
		FGkFx::FP& Sp = Fx.Add(0, At(P.x, P.y) + FVector(-20.0, Fx.Rng.FRandRange(-50.f, 50.f), Fx.Rng.FRandRange(0.f, 60.f)), FVector(0.0, Fx.Rng.FRandRange(-30.f, 30.f), Fx.Rng.FRandRange(300.f, 700.f)), 0.4f, 9.f, 2.f, Ki); Sp.Stretch = 6.f;
	}
	if ((P.st == gk::DRUSH || P.st == gk::RUN || P.st == gk::DASHF) && (Ticks % 2) == 0)
	{
		const FLinearColor Cg = P.st == gk::DRUSH ? FLinearColor(0.4f, 5.f, 1.4f) : FLinearColor(1.6f, 1.6f, 1.8f);
		for (int32 k = 0; k < 2; k++) { FGkFx::FP& St = Fx.Add(0, Chest + FVector(0.0, -P.face * 30.0, Fx.Rng.FRandRange(-70.f, 60.f)), FVector(0.0, -P.face * 500.0, 0.0), 0.16f, P.st == gk::DRUSH ? 12.f : 6.f, 2.f, Cg); St.Stretch = 12.f; }
	}
}

/** One clip's pose at a time, bone by bone in the mesh's own order; the root stays where the mesh has it. */
void AGkMatch::EvalClip(int32 i, UAnimSequence* A, float Time, TArray<FTransform>& Out)
{
	UPoseableMeshComponent* C = Body[i]; USkinnedAsset* SA = C ? C->GetSkinnedAsset() : nullptr; if (!SA || !A) { return; }
	const FReferenceSkeleton& RS = SA->GetRefSkeleton(); const int32 N = RS.GetNum(); Out = RS.GetRefBonePose();
	USkeleton* Sk = A->GetSkeleton(); if (!Sk) { return; }
	if (SkelFor[i] != Sk || SkelIdx[i].Num() != N)
	{
		SkelFor[i] = Sk; SkelIdx[i].SetNum(N); const FReferenceSkeleton& KS = Sk->GetReferenceSkeleton();
		for (int32 b = 0; b < N; b++) { SkelIdx[i][b] = KS.FindBoneIndex(RS.GetBoneName(b)); }
	}
	const FAnimExtractContext Ctx(double(Time), false);
	for (int32 b = 1; b < N; b++) { const int32 Si = SkelIdx[i][b]; if (Si != INDEX_NONE) { FTransform T; A->GetBoneTransform(T, FSkeletonPoseBoneIndex(Si), Ctx, false); Out[b] = T; } }
}

/** Where in a clip the blow lands, as a part of its length: the moment a hand or a foot is furthest out in front (or highest, for an uppercut). */
float AGkMatch::ImpactOf(int32 i, const FGkClip& C, bool bThrust)
{
	if (const float* H = (bThrust ? ThrustAt : HitAt).Find(C.Anim.Get())) { return FMath::Clamp(*H, C.From, C.To); }
	UPoseableMeshComponent* Mesh = Body[i]; USkinnedAsset* SA = Mesh ? Mesh->GetSkinnedAsset() : nullptr; if (!SA || !C.Anim) { return 0.5f; }
	const FReferenceSkeleton& RS = SA->GetRefSkeleton(); const int32 N = RS.GetNum(); const float Len = C.Anim->GetPlayLength();
	TArray<FTransform> Lo, Cs; Cs.SetNum(N); float Best = -1e9f, BestU = 0.5f;
	auto Reach = [&](float U)
	{
		EvalClip(i, C.Anim, U * Len, Lo); if (Lo.Num() != N) { return 0.f; }
		for (int32 b = 0; b < N; b++) { const int32 Pa = RS.GetParentIndex(b); Cs[b] = Pa >= 0 ? Lo[b] * Cs[Pa] : Lo[b]; }
		const FVector Pv = KeyBone[i][0] >= 0 ? Cs[KeyBone[i][0]].GetTranslation() : FVector(0.0, 0.0, 95.0); float R = -1e9f;
		// a shot or a beam leaves when both hands are thrust furthest forward together
		if (bThrust && KeyBone[i][1] >= 0 && KeyBone[i][2] >= 0) { return float(FMath::Min(Cs[KeyBone[i][1]].GetTranslation().Y, Cs[KeyBone[i][2]].GetTranslation().Y) - Pv.Y); }
		for (int32 k = 1; k <= 4; k++) { if (KeyBone[i][k] < 0) { continue; } const FVector L = Cs[KeyBone[i][k]].GetTranslation() - Pv; R = FMath::Max(R, float(k <= 2 ? L.Y + FMath::Max(0.0, L.Z - 55.0) : L.Y + (L.Z + 90.0) * 0.5)); }
		return R;
	};
	for (int32 k = 2; k <= 46; k++) { const float U = k / 48.f, R = Reach(U); if (R > Best) { Best = R; BestU = U; } }
	(bThrust ? ThrustAt : HitAt).Add(C.Anim.Get(), BestU);
	return FMath::Clamp(BestU, C.From, C.To);
}

/** -gkscript: poses the game as Saved/script.txt says and saves pictures of it, then quits. One command a line, parts split by |. */
void AGkMatch::RunScript()
{
	const FString Root = FPaths::ProjectSavedDir() / TEXT("shots/");
	if (ScriptWait > 0) { ScriptWait--; return; }
	if (!bPlaced) { return; }
	if (ScriptRunLeft > 0) { ScriptRunLeft--; StepOnce(ScriptIn[0], ScriptIn[1]); AnimClock++; return; }
	while (ScriptAt < Script.Num())
	{
		TArray<FString> A; Script[ScriptAt++].ParseIntoArray(A, TEXT("|")); if (A.Num() == 0 || A[0].StartsWith(TEXT("#"))) { continue; }
		auto Num = [&A](int32 k, int32 Def = 0) { return A.IsValidIndex(k) ? FCString::Atoi(*A[k]) : Def; };
		const FString& Op = A[0];
		if (Op == TEXT("lab"))
		{
			Screen = 4; bMenu = false; ScriptSlot = FName(*A[1]); LabFrame = Num(2); LabClock = LabFrame / 60.f; bLabDrop = false; LabTryAt = -2; LabTry = FGkClip();
			if (A.IsValidIndex(3) && A[3] == TEXT("none")) { bLabDrop = true; LabTryAt = -1; }
			else if (A.IsValidIndex(3))
			{
				LabTry.Anim = LoadObject<UAnimSequence>(nullptr, *A[3]); if (LabTry.Anim) { ClipKeep.Add(LabTry.Anim); bLabDrop = true; LabTryAt = 0; }
				// a fifth part: the frame number is a hundredth of the clip's length instead
				if (LabTry.Anim && A.IsValidIndex(4)) { LabClock = LabTry.Anim->GetPlayLength() * 0.995f * LabFrame / 100.f; ScriptNote = FString::Printf(TEXT("%.1fs of %.1fs"), LabClock, LabTry.Anim->GetPlayLength()); }
			}
			ScriptNote = FString::Printf(TEXT("%s  f%d"), *A[1], LabFrame);
		}
		else if (Op == TEXT("labui")) { bLabUi = true; LabOpen(); if (Num(1, -1) >= 0) { LabClick(1000 + Num(1)); } if (Num(2, -1) >= 0) { LabHot = 10000 + Num(2); } ScriptNote.Reset(); }
		else if (Op == TEXT("menu")) { Screen = Num(1); bMenu = Num(2) != 0; MenuAt = Num(3); Mode = 0; ScriptNote.Reset(); }
		else if (Op == TEXT("fight")) { Screen = 3; bMenu = false; Mode = Num(1, 1); Restart(); for (int32 k = 0; k < 400 && S.phase != gk::PH_FIGHT; k++) { gk::step(S, 0, 0); } ScriptNote.Reset(); CamD.bSnap = true; bBoxes = false; }
		else if (Op == TEXT("run")) { ScriptRunLeft = Num(1); ScriptIn[0] = Num(2); ScriptIn[1] = Num(3); if (ScriptRunLeft > 0) { return; } }
		else if (Op == TEXT("mv")) { const int32 Id = gk::moveByKey(TCHAR_TO_UTF8(*A[2])); if (Id >= 0) { gk::debugStart(S, Num(1), Id); } }
		else if (Op == TEXT("set"))
		{
			gk::Fighter& P = S.p[Num(1) & 1]; const int32 V = Num(3);
			if (A[2] == TEXT("bo")) { P.bo = V; if (V) { P.dr = 0; } } else if (A[2] == TEXT("hp")) { P.hp = V; } else if (A[2] == TEXT("dr")) { P.dr = V; } else if (A[2] == TEXT("sa")) { P.sa = V; } else if (A[2] == TEXT("x")) { P.x = V * gk::F; } else if (A[2] == TEXT("face")) { P.face = V; }
		}
		else if (Op == TEXT("dummy")) { Dummy = Num(1); }
		else if (Op == TEXT("sky")) { GkSkyForce = A.IsValidIndex(1) ? A[1] : FString(); GkTimeForce = A.IsValidIndex(2) ? FCString::Atod(*A[2]) : -1.0; }
		else if (Op == TEXT("time")) { if (SkyActor) { GkCall(SkyActor, TEXT("Set New Time"), nullptr, double(Num(1))); } }
		else if (Op == TEXT("stage"))
		{   // stage|index|1: go to that stage (the 1 loads its map afresh even if it is this one, to try it under another sky)
			const int32 To = Num(1);
			if (Stages.IsValidIndex(To) && (To != Setup.Stage || Num(2)))
			{
				if (Stages[To].Kind != 2 && Stages[To].Map == MapName()) { StartStage(To); }
				else { GkScriptResume = ScriptAt; Setup.Stage = To; Setup.Mode = 1; Setup.Level = Level; Setup.Dummy = Dummy; Setup.bGo = true; bScript = false; UGameplayStatics::OpenLevel(this, FName(*Stages[To].Map), true, TEXT("game=/Script/GekitotsuNative.GkGameMode")); return; }
			}
		}
		else if (Op == TEXT("anchor") && A.Num() >= 5 && Stages.IsValidIndex(Setup.Stage)) { FGkStage& G = Stages[Setup.Stage]; G.bFixed = true; G.At = FVector(FCString::Atod(*A[1]), FCString::Atod(*A[2]), FCString::Atod(*A[3])); G.Yaw = FCString::Atod(*A[4]); PlaceWait = 10.f; TryPlace(); CamD.bSnap = true; ScriptNote = PlaceNote; }
		else if (Op == TEXT("cam")) { bScout = A.Num() >= 6; if (bScout) { ScoutPos = FVector(FCString::Atod(*A[1]), FCString::Atod(*A[2]), FCString::Atod(*A[3])); ScoutRot = FRotator(FCString::Atod(*A[4]), FCString::Atod(*A[5]), 0.0); ScoutFov = A.Num() > 6 ? FCString::Atof(*A[6]) : 60.f; ScriptNote = FString::Printf(TEXT("%.0f,%.0f,%.0f p%.0f y%.0f"), ScoutPos.X, ScoutPos.Y, ScoutPos.Z, ScoutRot.Pitch, ScoutRot.Yaw); } }
		else if (Op == TEXT("rim")) { for (int32 k = 0; k < 3; k++) { RimTune[k] = Num(1 + k, 100) / 100.f; } if (A.Num() >= 7) { RimCol = FLinearColor(Num(4) / 100.f, Num(5) / 100.f, Num(6) / 100.f); } }
		else if (Op == TEXT("sun")) { GkApplySun(GetWorld(), Anchor.GetRotation().Rotator().Yaw + 180.0 + double(Num(1)), double(Num(2, -12)), double(Num(3, 100)) / 100.0, FLinearColor(Num(4, 100) / 100.f, Num(5, 100) / 100.f, Num(6, 100) / 100.f), A.Num() >= 7); }
		else if (Op == TEXT("lit")) { GkLitK = float(Num(1, 100)) / 100.f; }
		else if (Op == TEXT("edge")) { TArray<FString> N; for (int32 k = 1; k < A.Num(); k++) { N.Add(A[k]); } GkEdge(EdgeMid, N); }
		else if (Op == TEXT("lamp")) { LampI = float(Num(1, 5000)); LampScatter = Num(2, 300) / 100.f; if (BackLamp) { BackLamp->SetIntensity(LampI); BackLamp->SetVolumetricScatteringIntensity(LampScatter); BackLamp->SetLightColor(RimCol); } }
		else if (Op == TEXT("lens")) { LensF = Num(1, 18) / 10.f; }
		else if (Op == TEXT("air")) { AirKind = Num(1); }
		else if (Op == TEXT("bolt")) { BoltDue = 0.f; }
		else if (Op == TEXT("hud")) { bNoHud = Num(1) == 0; }
		else if (Op == TEXT("note")) { ScriptNote = A.IsValidIndex(1) ? A[1] : FString(); }
		else if (Op == TEXT("exec") && A.IsValidIndex(1)) { if (GEngine) { GEngine->Exec(GetWorld(), *A[1]); } }
		else if (Op == TEXT("palette")) { PaletteOverride = FMath::Clamp(Num(1, -1), -1, 5); }
		else if (Op == TEXT("storm")) { ScriptStorm = Num(1) / 100.f; }
		else if (Op == TEXT("wait")) { ScriptWait = Num(1, 1); return; }
		else if (Op == TEXT("shot")) { if (Cam) { const FVector CL = Cam->GetActorLocation(); const FRotator CR = Cam->GetActorRotation(); UE_LOG(LogTemp, Display, TEXT("GkCam %s %.0f,%.0f,%.0f p%.1f y%.1f fov %.1f"), *A[1], CL.X, CL.Y, CL.Z, CR.Pitch, CR.Yaw, Cam->GetCameraComponent()->FieldOfView); } IFileManager::Get().MakeDirectory(*Root, true); FScreenshotRequest::RequestScreenshot(Root + A[1] + TEXT(".png"), !bNoHud, false); ScriptWait = 3; return; }
		else if (Op == TEXT("quit")) { break; }
	}
	GkScriptResume = 0; bScript = false; if (GkFlyOn()) { return; } FFileHelper::SaveStringToFile(TEXT("done\n"), *(Root + TEXT("done.txt"))); FPlatformMisc::RequestExit(false);
}

/** The clip each state and move starts with, for every character (the lab changes them; its choices are read last). */
void AGkMatch::LoadClips()
{
	ClipKeep.Reset();
	static const TCHAR* Suffix[3] = { TEXT("_Kagu"), TEXT("_Noa"), TEXT("_Manny") };
	struct FPick { const char* Slot; const char* Path; bool bLoop; float From, To; };
#define GK_FA "/Game/FightAnimations/Animations/"
#define GK_V4 "/Game/UnarmFightAnimBundle/UnarmedFightingAnimationV4/Animation/IP/A_UnarmedFightingAnimationV4_"
#define GK_V3 "/Game/UnarmFightAnimBundle/UnarmedFightingAnimationV3/Animation/IP/A_UnarmedFightingAnimationV3_"
#define GK_V2 "/Game/UnarmFightAnimBundle/UnarmedFightingAnimationV2/Animation/IP/A_UnarmedFightingAnimationsV2_"
#define GK_SP "/Game/SpecialMoves_Energy/Animation/InPlace/Male/SpecialMove_"
#define GK_JK GK_V4 "Jump_Kick_IP", false, 0.25f, 0.8f
#define GK_JP GK_V4 "Jump_Punch1_IP", false, 0.25f, 0.8f
#define GK_SRK GK_V4 "Shoryuken_IP", false, 0.f, 1.f
#define GK_TAT GK_FA "Kick/A_HightKick_Round_R", false, 0.f, 1.f
#define GK_HOME GK_FA "Punch/Special/A_SuperMan_Punch_R", false, 0.f, 1.f
#define GK_JIN GK_FA "Punch/Special/A_BackFist_SuperMan_R", false, 0.f, 1.f
#define GK_S1 "/Game/Superhero_Combat/Animations/Style_One/"
#define GK_S2 "/Game/Superhero_Combat/Animations/Style_Two/"
	static const FPick Picks[] = {
		{ "idle", GK_FA "Base/A_Idle_Active", true, 0.00f, 1.00f }, { "walkf", GK_FA "Base/Walk/A_Move_F", true, 0.00f, 1.00f }, { "walkb", GK_FA "Base/Walk/A_Move_B", true, 0.00f, 1.00f },
		{ "crouch", GK_V4 "Squat_IP", false, 0.00f, 1.00f }, { "jump", GK_V4 "Jump_IP", false, 0.15f, 0.85f }, { "dashf", GK_FA "Base/Dodge/A_Dodge_F", false, 0.00f, 1.00f },
		{ "dashb", GK_FA "Base/Dodge/A_Dodge_B", false, 0.00f, 1.00f }, { "run", GK_V3 "Run_Fw_IP", true, 0.00f, 1.00f }, { "drush", GK_S2 "Anim_SC_Move_Forward_V2", true, 0.00f, 1.00f },
		{ "blockS", GK_FA "Block/InPlace/A_BlockHit_F_IP", false, 0.00f, 1.00f }, { "parry", GK_V3 "Block2_IP", true, 0.00f, 1.00f }, { "hitL", GK_FA "Hit/InPlace/A_HitHead_F_IP", false, 0.00f, 1.00f },
		{ "hitM", GK_FA "Hit/InPlace/A_HitBody_F_IP", false, 0.00f, 1.00f }, { "hitH", GK_FA "Hit/InPlace/A_HitHead_Up_IP", false, 0.00f, 1.00f }, { "hitLow", GK_FA "Hit/InPlace/A_HitLeg_L_IP", false, 0.00f, 1.00f },
		{ "hitCr", GK_FA "Crouch/Hit/A_Crourch_Hit_F", false, 0.00f, 1.00f }, { "ahit", GK_FA "Fall/A_FallBack", false, 0.08f, 0.42f }, { "kd", GK_FA "Fall/A_FallBack", false, 0.70f, 1.00f },
		{ "getup", GK_FA "Fall/StandUp/A_StandUp_Back", false, 0.00f, 1.00f }, { "mv_5LP", GK_FA "Punch/InPlace/Head/A_JabHead_L_IP", false, 0.00f, 1.00f }, { "mv_5MP", GK_FA "Punch/InPlace/Head/A_PunchHead_R_IP", false, 0.00f, 1.00f },
		{ "mv_5HP", GK_FA "Punch/InPlace/Head/A_HookHead_R_IP", false, 0.00f, 1.00f }, { "mv_2LP", GK_FA "Crouch/Punch/A_Crourch_Jab_L", false, 0.00f, 1.00f }, { "mv_2HP", GK_FA "Crouch/Punch/A_Crourch_Uppercut_R", false, 0.00f, 1.00f },
		{ "mv_CB2", GK_FA "Punch/Special/A_OverHand_R", false, 0.00f, 1.00f }, { "mv_5LK", GK_FA "Kick/A_LowKick_L", false, 0.00f, 1.00f }, { "mv_5MK", GK_FA "Kick/A_MiddleKick_R", false, 0.00f, 1.00f },
		{ "mv_5HK", GK_FA "Kick/A_HightKick_R", false, 0.00f, 1.00f }, { "mv_2LK", GK_FA "Crouch/Kick/A_Crourch_Kick_L", false, 0.00f, 1.00f }, { "mv_2MK", GK_FA "Crouch/Kick/A_Crourch_Kick_R", false, 0.00f, 1.00f },
		{ "mv_2HK", GK_FA "Crouch/Kick/A_Crourch_Kick_Round", false, 0.00f, 1.00f }, { "mv_TFK2", GK_FA "Kick/A_FrontKick_L", false, 0.00f, 1.00f }, { "mv_TFK3", GK_FA "Kick/A_SideKick", false, 0.00f, 1.00f },
		{ "mv_jLP", GK_S1 "Anim_SC_Punch", false, 0.00f, 1.00f }, { "mv_jMP", GK_S2 "Anim_SC_Punch", false, 0.00f, 1.00f }, { "mv_jHP", GK_S1 "Anim_SC_Punch_Down", false, 0.00f, 1.00f },
		{ "mv_jLK", GK_S2 "Anim_SC_Kick_v2", false, 0.00f, 1.00f }, { "mv_jMK", GK_S1 "Anim_SC_Kick", false, 0.00f, 1.00f }, { "mv_jHK", GK_V4 "Jump_Kick_IP", false, 0.25f, 0.80f },
		{ "mv_njHK", GK_V4 "Jump_Kick_IP", false, 0.25f, 0.80f }, { "mv_jUP", GK_S1 "Anim_SC_Kick_Up", false, 0.00f, 1.00f }, { "mv_AUP", GK_S2 "Anim_SC_Punch_up", false, 0.00f, 1.00f },
		{ "mv_ABK", GK_S2 "Anim_SC_Kick_v2", false, 0.00f, 1.00f }, { "mv_jDN", GK_S2 "Anim_SC_Kick_down_v2", false, 0.00f, 1.00f }, { "mv_jBK", GK_S1 "Anim_SC_Kick", false, 0.00f, 1.00f },
		{ "mv_ASPK", GK_S2 "Anim_SC_Kick_down_v2", false, 0.00f, 1.00f }, { "mv_ATAT", GK_S2 "Anim_SC_Kick_Double", false, 0.00f, 1.00f }, { "mv_ATAT_OD", GK_S2 "Anim_SC_Kick_Double", false, 0.00f, 1.00f },
		{ "mv_HOME", GK_S1 "Anim_SC_Move_Forward", true, 0.00f, 1.00f }, { "mv_HOME_OD", GK_S1 "Anim_SC_Move_Forward", true, 0.00f, 1.00f }, { "mv_AHOME", GK_S1 "Anim_SC_Move_Forward", true, 0.00f, 1.00f },
		{ "mv_AHOME_OD", GK_S1 "Anim_SC_Move_Forward", true, 0.00f, 1.00f }, { "mv_DI", GK_FA "Punch/Special/A_SuperMan_Punch_R", false, 0.00f, 1.00f }, { "mv_DREV", GK_FA "Punch/Special/A_SuperMan_Elbow_R", false, 0.00f, 1.00f },
		{ "mv_TK", GK_FA "Kick/A_HightKick_L", false, 0.00f, 1.00f }, { "mv_FSK", GK_FA "Kick/A_Round_SideKick_Mid", false, 0.00f, 1.00f }, { "mv_HAD_H", GK_SP "004_00_All_IP", false, 0.00f, 1.00f },
		{ "mv_HAD_OD", GK_SP "004_00_All_IP", false, 0.00f, 1.00f }, { "mv_SRK_L", GK_V4 "Shoryuken_IP", false, 0.00f, 1.00f }, { "mv_SRK_M", GK_V4 "Shoryuken_IP", false, 0.00f, 1.00f },
		{ "mv_SRK_H", GK_V4 "Shoryuken_IP", false, 0.00f, 1.00f }, { "mv_SRK_OD", GK_V4 "Shoryuken_IP", false, 0.00f, 1.00f }, { "mv_QSRK", GK_V4 "Shoryuken_IP", false, 0.00f, 1.00f },
		{ "mv_TAT_L", GK_FA "Kick/A_HightKick_Round_R", false, 0.00f, 1.00f }, { "mv_TAT_M", GK_FA "Kick/A_HightKick_Round_R", false, 0.00f, 1.00f }, { "mv_TAT_H", GK_FA "Kick/A_HightKick_Round_R", false, 0.00f, 1.00f },
		{ "mv_TAT_OD", GK_FA "Kick/A_HightKick_Round_R", false, 0.00f, 1.00f }, { "mv_QTAT", GK_FA "Kick/A_HightKick_Round_R", false, 0.00f, 1.00f }, { "mv_DL_H", GK_FA "Kick/A_LowKick_R_Round_v2", false, 0.00f, 1.00f },
		{ "mv_DL_OD", GK_FA "Kick/A_LowKick_R_Round_v2", false, 0.00f, 1.00f }, { "mv_QDL", GK_FA "Kick/A_LowKick_R_Round_v2", false, 0.00f, 1.00f }, { "mv_JIN_L", GK_FA "Kick/A_FrontKick_Havy", false, 0.00f, 1.00f },
		{ "mv_JIN_M", GK_FA "Kick/A_FrontKick_Havy", false, 0.00f, 1.00f }, { "mv_JIN_H", GK_FA "Kick/A_FrontKick_Havy", false, 0.00f, 1.00f }, { "mv_JIN_OD", GK_FA "Kick/A_FrontKick_Havy", false, 0.00f, 1.00f },
		{ "mv_KAZE", GK_FA "Kick/A_LowKick_R_Round", false, 0.00f, 1.00f }, { "mv_KAZE_OD", GK_FA "Kick/A_LowKick_R_Round", false, 0.00f, 1.00f }, { "mv_GORAI", GK_FA "Kick/A_AxeKick", false, 0.00f, 1.00f },
		{ "mv_GORAI_OD", GK_FA "Kick/A_AxeKick", false, 0.00f, 1.00f }, { "mv_SENKA", GK_FA "Kick/A_Round_SideKick_Hight", false, 0.00f, 1.00f }, { "mv_SENKA_OD", GK_FA "Kick/A_Round_SideKick_Hight", false, 0.00f, 1.00f },
		{ "mv_KASAI", GK_FA "Kick/A_SideKick_Havy", false, 0.00f, 1.00f }, { "mv_SA1", GK_SP "000_00_All_IP", false, 0.00f, 1.00f }, { "crumple", GK_FA "Fall/A_FallFront", false, 0.00f, 1.00f }, { "mv_SA2", GK_SP "005_00_All_IP", false, 0.00f, 1.00f },
		{ "mv_SA3", GK_SP "008_00_All_IP", false, 0.00f, 1.00f }, { "mv_CA", GK_SP "006_00_All_IP", false, 0.00f, 1.00f } };
	for (int32 Set = 0; Set < 3; Set++)
	{
		Clips[Set].Reset(); int32 Missing = 0;
		for (const FPick& Pk : Picks)
		{
			const FString Full = FString(ANSI_TO_TCHAR(Pk.Path)) + Suffix[Set]; const FString Obj = Full + TEXT(".") + FPaths::GetBaseFilename(Full);
			if (UAnimSequence* A = LoadObject<UAnimSequence>(nullptr, *Obj)) { A->bForceRootLock = true; FGkClip C; C.Anim = A; C.bLoop = Pk.bLoop; C.From = Pk.From; C.To = Pk.To; Clips[Set].Add(FName(ANSI_TO_TCHAR(Pk.Slot)), C); ClipKeep.Add(A); }
			else { Missing++; if (Missing < 4) { UE_LOG(LogTemp, Warning, TEXT("GkClip: %s is not there"), *Obj); } }
		}
		UE_LOG(LogTemp, Display, TEXT("GkClip: %d clips for set %d, %d missing"), Clips[Set].Num(), Set, Missing);
	}
	LoadPicks();
}

const AGkMatch::FGkClip* AGkMatch::ClipFor(int32 Set, FName Pose, int32 MoveId) const
{
	if (Set < 0 || Set > 2) { return nullptr; }
	const FName Slot = MoveId >= 0 && MoveClip.IsValidIndex(MoveId) ? MoveClip[MoveId] : Pose;
	const FGkClip* Own = Clips[Set].Find(Slot); return (Own && Own->Anim) ? Own : nullptr;
}

/** Vefects' Combat Flipbook VFX: the hit, guard and dust systems used, and the sounds that go with them. */
void AGkMatch::LoadVfx()
{
	const FString H = TEXT("/Game/Vefects/Combat_Flipbook_VFX/VFX/Hit/Particles/Once/"), D = TEXT("/Game/Vefects/Combat_Flipbook_VFX/VFX/Dust/Particles/Once/");
	auto Sys = [this](const FString& Dir, const TCHAR* Name) { if (UNiagaraSystem* N = LoadObject<UNiagaraSystem>(nullptr, *(Dir + Name + TEXT(".") + Name))) { Vfx.Add(FName(Name), N); } else { UE_LOG(LogTemp, Warning, TEXT("GkVfx: %s is not there"), Name); } };
	for (const TCHAR* N : { TEXT("NS_Hit_01_Once"), TEXT("NS_Hit_04_Once"), TEXT("NS_Hit_06_Directional_Once"), TEXT("NS_Radial_Spiky_Hit_01"), TEXT("NS_Block_Radial_01_Once"), TEXT("NS_Radial_Burst") }) { Sys(H, N); }
	for (const TCHAR* N : { TEXT("NS_Directional_Dust_Jump_01_Once"), TEXT("NS_Directional_Dust_01_Once"), TEXT("NS_Directional_Dust_03_Once_Fast"), TEXT("NS_Directional_Dust_03_Once_Fast_Flip") }) { Sys(D, N); }
	// the sounds: every take of the old-school shonen pack that was brought in, grouped by its sound ("Melee_06" holds Melee_06_1 to _6)
	{
		TArray<FAssetData> Found; FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPath(FName(TEXT("/Game/GkSfx")), Found, false);
		for (const FAssetData& A : Found)
		{
			FString Name = A.AssetName.ToString(); int32 Cut = INDEX_NONE; if (!Name.FindLastChar(TEXT('_'), Cut)) { continue; }
			if (USoundBase* Wv = Cast<USoundBase>(A.GetAsset())) { SndKeep.Add(Wv); SndSets.FindOrAdd(FName(*Name.Left(Cut))).Add(Wv); }
		}
		TArray<FAssetData> Tunes; FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPath(FName(TEXT("/Game/GkMusic")), Tunes, false);
		for (const FAssetData& A : Tunes) { if (USoundBase* Wv = Cast<USoundBase>(A.GetAsset())) { Songs.Add(Wv); } }
		UE_LOG(LogTemp, Display, TEXT("GkSfx: %d sounds in %d sets, %d songs"), SndKeep.Num(), SndSets.Num(), Songs.Num());
	}
	// the Special Moves kit's own energy effects: its shot, its beam, its auras
	for (const TCHAR* N : { TEXT("NS_Beam"), TEXT("NS_EnergyShot"), TEXT("NS_Aura"), TEXT("NS_Aura2"), TEXT("NS_HandAura"), TEXT("NS_Hit"), TEXT("NS_Ground_ShockWave"), TEXT("NS_Thunder") }) { Sys(TEXT("/Game/SpecialMoves_Energy/Demo/VFX/Niagara/"), N); }
	UE_LOG(LogTemp, Display, TEXT("GkVfx: %d effects, %d sounds"), Vfx.Num(), Sfx.Num());
}

/** Plays one effect at a place in the fight's own space, turned to the way the blow travels (Dir +1 or -1 along the stage, 0 for none), with its sound. False if the effect is not there. */
bool AGkMatch::PlayVfx(const TCHAR* Name, const FVector& Local, float Scale, float Dir, const TCHAR* Sound, float Volume)
{
	if (Sound && Screen == 3) { if (TObjectPtr<USoundBase>* Sn = Sfx.Find(FName(Sound))) { UGameplayStatics::PlaySound2D(this, *Sn, Volume); } }
	TObjectPtr<UNiagaraSystem>* Sy = Vfx.Find(FName(Name)); if (!Sy || !*Sy) { return false; }
	const FQuat Turn = Anchor.GetRotation() * FRotator(0.0, Dir > 0.f ? 90.0 : Dir < 0.f ? -90.0 : 0.0, 0.0).Quaternion();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, *Sy, Wd(Local), Turn.Rotator(), FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
	return true;
}

void AGkMatch::Present(float Real)
{
	if (bForestLook && bPlaced) { UpdateForestLook(Real); }
	for (int32 i = 0; i < 2; i++) { ShowFighter(i); }
	// which place is shown: training is fought in the measured room, everything else in the wasteland
	const int32 Place = Kind;
	if (Place != ShownPlace)
	{
		ShownPlace = Place;
		for (USceneComponent* C : GridParts) { if (C) { C->SetVisibility(Place == 0); } }
		for (USceneComponent* C : StageParts) { if (C) { C->SetVisibility(Place == 1); } }
		const bool bSkyOwns = Place != 0 && (SkyActor != nullptr || bOwnLight);   // one system owns the sun, sky light and fog at a time
		if (Fog && Fog->GetComponent()) { Fog->GetComponent()->SetVisibility(Place == 1 && !bSkyOwns); }
		for (AActor* L : RoomLights) { if (L) { L->SetActorHiddenInGame(bSkyOwns); if (ULightComponentBase* C = L->FindComponentByClass<ULightComponentBase>()) { C->SetVisibility(!bSkyOwns); } } }
		if (RoomAir) { RoomAir->SetVisibility(!bSkyOwns); }
		if (SkyActor) { SkyActor->SetActorHiddenInGame(!bSkyOwns); TArray<USceneComponent*> Cs; SkyActor->GetComponents(Cs); for (USceneComponent* C : Cs) { C->SetVisibility(bSkyOwns); } }
		if (BackLamp) { BackLamp->SetVisibility(Place != 0 && LampI > 0.f); }
		if (NinjaLevel) { NinjaLevel->SetShouldBeVisible(Place != 0); }
	}
	// The weather follows the fight: a red dusk while both stand strong, a storm as one of them nears the end. It starts
	// to build when either drops below 60% health, is at full strength at 25%, and never falls under 65% from round three.
	{
		const float HpMin = FMath::Min(S.p[0].hp, S.p[1].hp) / 1000.f;
		float Want = FMath::Clamp((0.78f - HpMin) / 0.6f, 0.f, 1.f);
		if (S.round == 2) { Want = FMath::Max(Want, 0.32f); } if (S.round >= 3) { Want = FMath::Max(Want, 0.8f); }
		if (Place == 0 || Screen != 3) { Want = 0.f; }
		if (bForestLook && PaletteOverride >= 0) { Want = PaletteWeights[2]; }
		if (ScriptStorm >= 0.f) { Want = ScriptStorm; }
		Storm = FMath::Lerp(Storm, Want, FMath::Clamp(Real * 1.2f, 0.f, 1.f));
		const int32 Stage = Want < 0.05f ? 0 : Want < 0.3f ? 1 : Want < 0.6f ? 2 : 3;
		SetWeather(Stage, Stage == 0 ? 2.0 : 3.0);
		// the storm's own rain and lightning, drawn the way a cartoon draws them: long bright strokes of rain in two depths,
		// white lines of wind across the picture, rings where the drops land, mist driven along the ground, and forked bolts
		BoltT = FMath::Max(0.f, BoltT - Real * 60.f);
		if (Place != 0 && Storm > 0.12f)
		{
			WindDue -= Real; int32 Guard = 0;
			while (WindDue <= 0.f && Guard++ < 6)
			{   // lines of wind
				WindDue += 0.05f / Storm;
				FGkFx::FP& Wl = Fx.Add(0, FVector(Fx.Rng.FRandRange(-500.f, 60.f), CamPos.Y + Fx.Rng.FRandRange(-300.f, 1500.f), Fx.Rng.FRandRange(20.f, 520.f)), FVector(0.0, -Fx.Rng.FRandRange(2200.f, 3600.f), Fx.Rng.FRandRange(-60.f, 60.f)), Fx.Rng.FRandRange(0.18f, 0.34f), Fx.Rng.FRandRange(3.f, 6.f), 2.f, FLinearColor(1.5f, 1.7f, 2.1f) * (0.5f + Storm), 0.7f); Wl.Stretch = 60.f; Wl.bSwell = true;
				if (Storm > 0.3f) { FGkFx::FP& Mi = Fx.Add(1, FVector(Fx.Rng.FRandRange(-380.f, 120.f), CamPos.Y + Fx.Rng.FRandRange(0.f, 1500.f), Fx.Rng.FRandRange(10.f, 120.f)), FVector(0.0, -Fx.Rng.FRandRange(500.f, 900.f) * Storm, Fx.Rng.FRandRange(0.f, 30.f)), Fx.Rng.FRandRange(1.6f, 2.6f), Fx.Rng.FRandRange(200.f, 320.f), Fx.Rng.FRandRange(360.f, 520.f), bForestLook ? FLinearColor(0.40f, 0.35f, 0.64f) : FLinearColor(0.5f, 0.56f, 0.68f), 0.1f + 0.08f * Storm); Mi.bSwell = true; }
			}
		}
		if (Place != 0 && Storm > 0.35f)
		{
			RainDue -= Real; int32 Guard = 0;
			while (RainDue <= 0.f && Guard++ < 12)
			{
				RainDue += 0.0075f / Storm; const bool bNear = Fx.Rng.FRand() < 0.22f;
				FGkFx::FP& Dr = Fx.Add(0, FVector(bNear ? Fx.Rng.FRandRange(-760.f, -420.f) : Fx.Rng.FRandRange(-380.f, 160.f), CamPos.Y + Fx.Rng.FRandRange(-1100.f, 1500.f), Fx.Rng.FRandRange(320.f, 760.f)), FVector(0.0, -1050.0 * Storm, bNear ? -3300.0 : -2500.0), bNear ? 0.24f : 0.32f, bNear ? 13.f : 8.f, bNear ? 11.f : 6.f, bNear ? FLinearColor(1.6f, 2.1f, 3.f) : FLinearColor(2.4f, 3.f, 4.2f), bNear ? 0.55f : 0.85f); Dr.Stretch = bNear ? 34.f : 26.f;
			}
			SplashDue -= Real; Guard = 0;
			while (SplashDue <= 0.f && Guard++ < 6)
			{   // where the drops land: a ring that opens on the ground and two beads thrown up from it
				SplashDue += 0.02f / Storm; FVector G(Fx.Rng.FRandRange(-260.f, 90.f), CamPos.Y + Fx.Rng.FRandRange(-900.f, 900.f), 3.0);
				if (bForestLook) { G = FloorPoint(G) + FVector(0.0, 0.0, 3.0); }
				Fx.Add(0, G, FVector::ZeroVector, 0.16f, 4.f, 30.f, FLinearColor(1.4f, 1.8f, 2.6f), 0.6f);
				for (int32 k = 0; k < 2; k++) { FGkFx::FP& Bd = Fx.Add(0, G, FVector(0.0, Fx.Rng.FRandRange(-90.f, 90.f), Fx.Rng.FRandRange(120.f, 260.f)), 0.22f, 4.f, 1.5f, FLinearColor(2.f, 2.6f, 3.6f)); Bd.Grav = 1100.f; Bd.Stretch = 3.f; }
			}
			BoltDue -= Real;
			if (BoltDue <= 0.f && Storm > 0.55f)
			{   // a bolt: a jagged line of light down the sky behind the fight, with a fork or two off it, and a glow where it comes down
				double By = CamPos.Y + Fx.Rng.FRandRange(-800.f, 800.f), Bz = 980.0; const double Bx = Fx.Rng.FRandRange(240.f, 420.f);
				for (int32 Seg = 0; Seg < 9; Seg++)
				{
					const double Ny = By + Fx.Rng.FRandRange(-110.f, 110.f);
					FGkFx::FP& Bt = Fx.Add(0, FVector(Bx, (By + Ny) * 0.5, Bz - 55.0), FVector(0.0, (Ny - By) * 0.02, -1.0), 0.26f, 9.f, 5.f, FLinearColor(11.f, 9.f, 20.f)); Bt.Streak = FVector(0.0, Ny - By, -110.0) * 1.15; Bt.Vel = FVector::ZeroVector;
					Fx.Add(0, FVector(Bx + 5.0, (By + Ny) * 0.5, Bz - 55.0), FVector::ZeroVector, 0.2f, 190.f, 150.f, FLinearColor(0.7f, 0.85f, 1.5f), 0.5f);
					if (Seg >= 2 && Fx.Rng.FRand() < 0.4f)
					{   // a fork
						double Fy = Ny, Fz = Bz - 110.0; const double Side = Fx.Rng.FRand() < 0.5f ? -1.0 : 1.0;
						for (int32 F2 = 0; F2 < 3; F2++) { const double Gy = Fy + Side * Fx.Rng.FRandRange(50.f, 130.f); FGkFx::FP& Fk = Fx.Add(0, FVector(Bx, (Fy + Gy) * 0.5, Fz - 40.0), FVector(0.0, (Gy - Fy) * 0.03, -1.0), 0.2f, 5.f, 3.f, FLinearColor(8.f, 9.f, 13.f)); Fk.Streak = FVector(0.0, Gy - Fy, -80.0) * 1.15; Fk.Vel = FVector::ZeroVector; Fy = Gy; Fz -= 80.0; }
					}
					By = Ny; Bz -= 110.0;
				}
				Fx.Add(0, FVector(Bx, By, 30.0), FVector::ZeroVector, 0.3f, 300.f, 620.f, FLinearColor(2.4f, 2.8f, 4.5f), 0.8f);
				Snd(TEXT("Thunder_01"), 0.4f + 0.25f * Storm); Flash = FMath::Max(Flash, 0.6f + 0.5f * Storm); Shake = FMath::Max(Shake, 2.f + 3.f * Storm); BoltT = 7.f; BoltDue = Fx.Rng.FRandRange(1.6f, 5.f) / Storm;
			}
		}
		// the rain is heard as well as seen, and the menus have their own pulse
		{
			auto Loop = [this](TObjectPtr<UAudioComponent>& C, const TCHAR* Set, float Vol)
			{
				if (Vol > 0.01f && !C) { if (const TArray<USoundBase*>* T = SndSets.Find(FName(Set))) { if (T->Num()) { C = UGameplayStatics::SpawnSound2D(this, (*T)[0], Vol, 1.f, 0.f, nullptr, false, false); } } }
				if (C) { if (Vol <= 0.01f) { C->Stop(); C = nullptr; } else { C->SetVolumeMultiplier(Vol); } }
			};
			// outdoors: a thin wind always, a rising gale and rain that goes from light to a downpour as the storm builds; the training room hears rain on its roof
			const bool bOut = Place != 0 && !bScript; const float Duck = Screen == 3 ? 1.f : 0.6f;
			Loop(WindLoop, TEXT("LoopWindLight_01"), bOut ? 0.6f * (1.f - 0.6f * Storm) * Duck : 0.f);
			Loop(GaleLoop, TEXT("LoopWindHeavy_01"), bOut ? FMath::Clamp((Storm - 0.2f) * 0.2f, 0.f, 0.13f) * Duck : 0.f);
			Loop(RainLoop, TEXT("LoopRainLight_01"), bOut ? FMath::Clamp((Storm - 0.3f) * 3.f, 0.f, 1.f) * FMath::Clamp(1.5f - Storm * 1.2f, 0.25f, 1.f) * 0.45f * Duck : 0.f);
			Loop(PourLoop, TEXT("LoopRainHeavy_01"), bOut ? FMath::Clamp((Storm - 0.55f) * 0.5f, 0.f, 0.17f) * Duck : 0.f);
			Loop(RoomLoop, TEXT("LoopRainInside_01"), (Place == 0 && !bScript) ? 0.5f : 0.f);
			// the songs play one after another in a random order for as long as the game is open, a little lower under a fight
			if (!bScript && Songs.Num() > 0)
			{
				if (!Music || !Music->IsPlaying())
				{
					int32 Next = Fx.Rng.RandRange(0, Songs.Num() - 1); if (Songs.Num() > 1 && Next == LastSong) { Next = (Next + 1 + Fx.Rng.RandRange(0, Songs.Num() - 2)) % Songs.Num(); }
					LastSong = Next; SongGain = Songs[Next]->GetName().Contains(TEXT("Song2")) ? 1.28f : Songs[Next]->GetName().Contains(TEXT("Song6")) ? 0.96f : 1.f; if (Music) { Music->Stop(); } Music = UGameplayStatics::SpawnSound2D(this, Songs[Next], 0.5f, 1.f, 0.f, nullptr, false, false);
				}
				if (Music) { MusicVol = FMath::Lerp(MusicVol, (Screen == 3 && !bMenu) ? (S.freeze > 0 ? 0.5f : 0.74f) : 0.85f, FMath::Clamp(Real * 3.f, 0.f, 1.f)); Music->SetVolumeMultiplier(MusicVol * SongGain); }
			}
		}
		// a super dims the world behind the fighters for as long as it holds the screen; the weather's own light is untouched
		const bool bImpact = ImpactT[0] > 12 || ImpactT[1] > 12;   // the world dims for the first beats of a drive impact as it does for a super
		Dim = FMath::Lerp(Dim, S.freeze > 0 ? 1.f : bImpact ? 0.7f : 0.f, FMath::Clamp(Real * 14.f, 0.f, 1.f));
		Flash *= FMath::Pow(0.0008f, Real);
		if (Cam)
		{
			FPostProcessSettings& PP = Cam->GetCameraComponent()->PostProcessSettings; const double G = (1.0 - 0.55 * Dim) * (1.0 - 0.2 * Storm * Storm) + (bForestLook ? 0.55 : 1.5) * Flash;
			PP.bOverride_ColorGain = true; PP.ColorGain = FVector4(G * ForestGrade.R * (1.0 - 0.1 * Storm), G * ForestGrade.G, G * ForestGrade.B * (1.0 + 0.08 * Dim + 0.1 * Storm), 1.0);
			PP.bOverride_ColorSaturation = true; const double Sat = 1.0 - 0.3 * Storm; PP.ColorSaturation = FVector4(Sat, Sat, Sat, 1.0);
		}
		Fx.DustCol = Place != 0 ? FMath::Lerp(FLinearColor(0.62f, 0.47f, 0.33f), FLinearColor(0.36f, 0.38f, 0.48f), Storm) : FLinearColor(0.45f, 0.47f, 0.52f);
	}
	if ((Kind == 1 || (bForestLook && bPlaced)) && Screen == 3 && !bMenu)
	{
		const float DustDt = FMath::Min(Real, 0.05f);
		Fx.CanyonHaze(CamPos.Y, DustDt, bForestLook ? FloorPoint(FVector(0.0, CamPos.Y, 0.0)).Z : 0.0,
			bForestLook ? FMath::Lerp(ForestRim, FLinearColor::White, 0.5f) : FLinearColor(0.83f, 0.63f, 0.43f), Storm);
		for (const gk::Fighter& Fighter : S.p)
		{
			if (Fighter.y <= 0 && Fighter.vx != 0)
			{
				Fx.StirCanyonHaze(At(Fighter.x, 0), Fighter.vx * Cm * 60.0, DustDt);
			}
		}
	}
	Fx.Air(CamPos.Y, Real, Place != 0 && (Storm < 0.5f || AirKind == 1 || AirKind == 2 || AirKind == 7), AirKind, Storm); Fx.Tick(Real);
	// the balls the ground fog feels: one at each fighter's hips and one at his feet, and four more that ride the shots in
	// the air, or are strung along the great beam while it burns
	{
		int32 Beamer = -1; for (int32 i = 0; i < 2; i++) { const gk::Fighter& P = S.p[i]; if (P.st == gk::ATK && P.mv == gk::moveByKey("SA3")) { const gk::Move& M = gk::move(P.mv); if (P.mf >= M.hits[0].s - 2 && P.mf <= M.lastAct + 3) { Beamer = i; } } }
		// the copied level's own script belongs to FluidNinja's demo and has nothing left to run on
		if (NinjaLevel && !bNinjaQuiet) { if (ULevel* Lv = NinjaLevel->GetLoadedLevel()) { if (AActor* Sc = Lv->GetLevelScriptActor()) { Sc->SetActorTickEnabled(false); bNinjaQuiet = true; } } }
		static const FName Bones[6] = { TEXT("hand_l"), TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("head"), TEXT("spine_03") };
		for (int32 k = 0; k < Stirrers.Num(); k++)
		{
			AActor* B = Stirrers[k]; if (!B) { continue; }
			if (Kind == 1)
			{
				const int32 Who = k / 2;
				const gk::Fighter& P = S.p[Who];
				FVector Point = Wd(FVector(0.0, 0.0, -6000.0));
				if (Screen == 3 && !bMenu && P.y <= 0 && P.vx != 0)
				{
					const bool bDash = P.st == gk::DASHF || P.st == gk::DASHB || P.st == gk::DRUSH;
					const double Side = (k & 1) ? 1.0 : -1.0;
					const double Direction = P.vx > 0 ? 1.0 : -1.0;
					const double Along = bDash ? -Direction * ((k & 1) ? 58.0 : 18.0) : Side * 15.0;
					Point = Wd(At(P.x, 0) + FVector(Side * 15.0, Along, 14.0));
				}
				B->SetActorLocation(Point, false);
				continue;
			}
			if (k < 12)
			{   // a ball on each hand and foot, the head and the chest of both fighters
				const int32 Who = k / 6; const gk::Fighter& P = S.p[Who]; UPoseableMeshComponent* C = Body[Who];
				FVector Wp = Wd(At(P.x, P.y) + FVector(0.0, 0.0, 90.0)); if (C && C->GetBoneIndex(Bones[k % 6]) != INDEX_NONE) { Wp = C->GetBoneLocationByName(Bones[k % 6], EBoneSpaces::WorldSpace); }
				B->SetActorLocation(Wp, false); continue;
			}
			const int32 q = k - 12;
			if (Beamer >= 0) { const gk::Fighter& P = S.p[Beamer]; const double Run = 120.0 + FMath::Fmod(double(Ticks) * 55.0 + q * 420.0, 1700.0); B->SetActorLocation(Wd(At(P.x, P.y + 56 * gk::F) + FVector(0.0, P.face * Run, 0.0)), false); }
			else if (q < S.nshots && !bMenu) { B->SetActorLocation(Wd(At(S.shot[q].x, S.shot[q].y)), false); }
			else
			{
				// Reuse idle projectile stirrers for two low wake points per fighter.
				const gk::Fighter& P = S.p[q / 2];
				const bool bDash = P.st == gk::DASHF || P.st == gk::DASHB || P.st == gk::DRUSH;
				FVector Point(0.0, 0.0, -6000.0);
				if (Screen == 3 && !bMenu && bDash && P.y <= 0 && P.vx != 0)
				{
					const double Direction = P.vx > 0 ? 1.0 : -1.0;
					const double Trail = (q & 1) ? 78.0 : 26.0;
					Point = At(P.x, P.y) + FVector(0.0, -Direction * Trail, 22.0);
				}
				B->SetActorLocation(Wd(Point), false);
			}
		}
	}
	for (int32 k = 0; k < 4; k++)
	{
		if (ShotMesh[k]) { ShotMesh[k]->SetVisibility(false); }
		const bool bOn = k < S.nshots && !bMenu; const int32 Want = !bOn ? 0 : S.shot[k].k == gk::moveByKey("HAD_OD") ? 2 : 1;
		if (ShotKind[k] != Want)
		{
			if (ShotFx[k]) { ShotFx[k]->Deactivate(); ShotFx[k]->DestroyComponent(); ShotFx[k] = nullptr; }
			ShotKind[k] = Want;
			// the kit's energy shot; the overdrive one is the kit's beam
			if (TObjectPtr<UNiagaraSystem>* Sy = Want ? Vfx.Find(FName(Want == 2 ? TEXT("NS_Beam") : TEXT("NS_EnergyShot"))) : nullptr)
			{
				ShotFx[k] = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, *Sy, Wd(At(S.shot[k].x, S.shot[k].y, -10.0)), FRotator::ZeroRotator, FVector(Want == 2 ? 0.55f : 1.2f), false, true, ENCPoolMethod::None);
			}
		}
		if (bOn)
		{
			const FVector Sp = At(S.shot[k].x, S.shot[k].y, -10.0); const FLinearColor Kc = (S.shot[k].o & 1) ? FLinearColor(1.5f, 3.5f, 8.f) : FLinearColor(7.f, 3.6f, 0.9f);
			if (Want == 2)
			{   // the overdrive shot is a beam: it runs from his hands for as long as he holds the pose, then trails its head
				const gk::Shot& Q = S.shot[k]; const gk::Fighter& Ow = S.p[Q.o & 1]; const bool bHeld = Ow.st == gk::ATK && Ow.mv == Q.k;
				const int32 X0 = bHeld ? Ow.x + Ow.face * 34 * gk::F : Q.x - Q.dir * 230 * gk::F; const FVector A0 = At(X0, Q.y, -10.0);
				BeamDue -= Real; if (BeamDue <= 0.f) { BeamDue = 1.f / 60.f; Beam(A0, float(Q.dir), FMath::Abs(float(Sp.Y - A0.Y)), 46.f, Kc); }
			}
			if (ShotFx[k]) { ShotFx[k]->SetWorldLocationAndRotation(Wd(Sp), Anchor.GetRotation() * FRotator(0.0, S.shot[k].dir > 0 ? 90.0 : -90.0, 0.0).Quaternion()); }
			else { Fx.Add(0, Sp, FVector::ZeroVector, 0.04f, 54.f, 46.f, FLinearColor(3.6f, 3.4f, 3.f)); Fx.Add(0, Sp, FVector::ZeroVector, 0.05f, 120.f, 104.f, Kc * 0.22f); }
			{ FGkFx::FP& Tr = Fx.Add(0, Sp, FVector(0.0, Fx.Rng.FRandRange(-160.f, 160.f), Fx.Rng.FRandRange(-160.f, 160.f)), 0.3f, 7.f, 2.f, Kc * 1.2f); Tr.Drag = 2.f; }
		}
	}
	// the camera frames both fighters the way the browser does: wide enough for the two of them and for the higher one's head
	const bool bLabCam = Screen == 4;
	const double Ax = bLabCam ? -60.0 : S.p[0].x / double(gk::F), Bx = bLabCam ? 60.0 : S.p[1].x / double(gk::F), Hy = bLabCam ? 0.0 : FMath::Max(S.p[0].y, S.p[1].y) / double(gk::F);
	const double Tw = FMath::Max3(236.0, FMath::Min(480.0, FMath::Abs(Ax - Bx) + 150.0), FMath::Min(600.0, (Hy + 150.0) * 2.0)), Ty = Tw * 9.0 / 32.0 - 17.0, Unit = Cm * gk::F;
	const double HalfFov = FMath::DegreesToRadians(15.0), Dist = (Tw * 0.5 * Unit) / FMath::Tan(HalfFov);
	const double Edge = FMath::Max(0.0, (gk::STAGE / double(gk::F) + 30.0) - Tw * 0.5), Tx = FMath::Clamp((Ax + Bx) * 0.5, -Edge, Edge);
	const FVector Want(-Dist, Tx * Unit, Ty * Unit);
	{ const double Out = Want.X < CamPos.X ? 18.0 : 7.0; CamPos.X = FMath::Lerp(CamPos.X, Want.X, FMath::Clamp(double(Real) * Out, 0.0, 1.0)); CamPos.Y = FMath::Lerp(CamPos.Y, Want.Y, FMath::Clamp(double(Real) * 9.0, 0.0, 1.0)); CamPos.Z = FMath::Lerp(CamPos.Z, Want.Z, FMath::Clamp(double(Real) * (Want.Z > CamPos.Z ? 18.0 : 7.0), 0.0, 1.0)); }
	Shake *= FMath::Pow(0.85f, FMath::Clamp(Real * 60.f, 0.f, 4.f));

	// ---- the director (the browser game's camDirect): what the moment asks of the camera
	struct FWant { float Yaw, Pit, Roll, Zm, Fov, K, Pad; bool bSolo; } T = { float(FMath::Sin(Ticks * 0.013) * 1.8), 4.f, float(FMath::Sin(Ticks * 0.009) * 0.5), 1.f, 0.f, 0.1f, 0.f, false };
	auto Ez = [](float u) { u = FMath::Clamp(u, 0.f, 1.f); return u * u * (3.f - 2.f * u); };
	if (S.freeze > 0) { FrzMax = FMath::Max(FrzMax, S.freeze); } else { FrzMax = 0; }
	bool bDirected = false;
	if (Place == 0 || bMenu || Screen == 4) { bDirected = true; }   // the training room and the menu are watched plainly
	else if (S.phase == gk::PH_INTRO) { const float e = 1.f - Ez(S.pt / (gk::INTRO * 0.8f)); T.Yaw = -62.f * e; T.Pit = 4.f + 9.f * e; T.Zm = 1.f + 0.3f * e; T.Roll = -5.f * e; T.K = 0.3f; bDirected = true; }
	for (int32 i = 0; i < 2 && !bDirected; i++)
	{
		const gk::Fighter& P = S.p[i]; const gk::Fighter& O = S.p[1 - i]; const gk::Move* M = (P.st == gk::ATK && P.mv >= 0) ? &gk::move(P.mv) : nullptr; const float Fc = float(P.face);
		if (M && M->sup)
		{
			if (S.freeze > 0 && S.frzWho == i && FrzMax > 0)
			{   // the flash before a super: the camera comes round to his front and low, then back as it lets go
				const float u = Ez(1.f - float(S.freeze) / float(FrzMax)); const bool bBig = M->ca != 0;
				T.Yaw = Fc * (bBig ? -58.f + 100.f * u : -40.f + 66.f * u); T.Pit = bBig ? -9.f + 6.f * u : -6.f; T.Roll = Fc * (bBig ? 12.f - 20.f * u : 8.f - 12.f * u); T.Fov = bBig ? 0.f : 4.f * (1.f - u); T.K = 0.34f; T.bSolo = true;
			}
			else { T.Yaw = Fc * (M->ca ? 20.f : 13.f) * FMath::Sin(FMath::Min(3.1f, P.mf * 0.045f)); T.Roll = Fc * (M->ca ? 6.f : 4.f) * FMath::Cos(P.mf * 0.06f); T.Pit = P.y > 30 * gk::F ? -5.f : 1.f; T.K = 0.14f; }
			bDirected = true; break;
		}
		if (M && M->has_home && P.mf >= M->home[0] && !P.hmE && !(P.contact && !P.fr)) { T.Yaw = -Fc * 27.f; T.Roll = -Fc * 7.f; T.Pit = 1.f; T.Fov = 6.f; T.K = 0.3f; T.Pad = 44.f; bDirected = true; break; }
		if (P.st == gk::THROWING) { T.Yaw = Fc * 26.f * Ez(P.sf / 26.f); T.Roll = Fc * 4.f; T.Zm = 0.88f; T.Pit = 1.f; T.K = 0.16f; bDirected = true; break; }
		if (M && M->di) { T.Yaw = -Fc * 16.f; T.Roll = -Fc * 5.f; T.Zm = 0.9f; T.K = 0.22f; }
		else if (P.st == gk::DRUSH || P.st == gk::RUN) { T.Yaw += -Fc * 11.f; T.Roll += -Fc * 2.5f; T.Fov = 4.f; T.K = 0.14f; }
		else if (O.st == gk::AHIT && O.y > 46 * gk::F && P.st != gk::AHIT) { T.Yaw += (O.vx > 0 ? 1.f : -1.f) * 7.f; T.Pit = 0.f; }
	}
	if (!bDirected || S.phase == gk::PH_KO)
	{
		if (S.phase == gk::PH_KO && S.rw >= 0 && S.rw < 2 && Place != 0 && !bMenu)
		{   // the knockout: round to the winner's side, then a slow drift as he takes his pose
			const gk::Fighter& Wn = S.p[S.rw]; const gk::Fighter& Ls = S.p[1 - S.rw]; const float Side = Wn.x <= Ls.x ? -1.f : 1.f;
			if (S.pt < 150) { T.Yaw = Side * (46.f * Ez(S.pt / 46.f) + FMath::Min(18.f, S.pt * 0.1f)); T.Roll = Side * 9.f * (1.f - FMath::Clamp(S.pt / 80.f, 0.f, 1.f)); T.Pit = -3.f; T.K = 0.2f; T.bSolo = true; }
			else if (Wn.st == gk::WIN) { T.Yaw = -float(Wn.face) * (30.f - FMath::Min(52.f, (S.pt - 150) * 0.22f)); T.Pit = -4.f; T.K = 0.06f; T.bSolo = true; }
		}
		else if (S.freeze > 0 && Place != 0 && !bMenu) { T.Zm = 0.84f; T.Fov = 6.f; T.K = 0.3f; }   // the held beat of a perfect parry
	}
	if (Place == 0 || bMenu || Screen == 4) { T = { 0.f, 4.f, 0.f, 1.f, 0.f, 0.2f, 0.f, false }; }
	if (GkFlyOn() && Place != 0 && Screen == 3 && !bMenu && !bScript)
	{   // a free camera on the stage: mouse looks, WASD moves, Q and E go down and up, shift is quick and control is slow, Z and X change the lens, F writes where it is to Saved/fly.txt
		UWorld* FW = GetWorld(); APlayerController* PC = FW ? FW->GetFirstPlayerController() : nullptr;
		if (PC && Cam)
		{
			if (!bFly) { bFly = true; bScout = true; ScoutPos = Cam->GetActorLocation(); ScoutRot = Cam->GetActorRotation(); ScoutFov = float(GkFovBase); PC->SetShowMouseCursor(false); PC->SetInputMode(FInputModeGameOnly()); }
			float Dx = 0.f, Dy = 0.f; PC->GetInputMouseDelta(Dx, Dy);
			ScoutRot.Yaw += Dx * 0.6f; ScoutRot.Pitch = FMath::Clamp(ScoutRot.Pitch + Dy * 0.6f, -89.f, 89.f); ScoutRot.Roll = 0.f;
			static double FlySpeed = 3000.0; if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) { FlySpeed = FMath::Min(40000.0, FlySpeed * 1.4); } if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) { FlySpeed = FMath::Max(150.0, FlySpeed / 1.4); }
			const double Sp = FlySpeed * (PC->IsInputKeyDown(EKeys::LeftShift) ? 4.0 : PC->IsInputKeyDown(EKeys::LeftControl) ? 0.15 : 1.0) * Real;
			const FVector Fwd = ScoutRot.Vector(), Rgt = FRotationMatrix(FRotator(0.0, ScoutRot.Yaw, 0.0)).GetScaledAxis(EAxis::Y); FVector Mv = FVector::ZeroVector;
			if (PC->IsInputKeyDown(EKeys::W)) { Mv += Fwd; } if (PC->IsInputKeyDown(EKeys::S)) { Mv -= Fwd; } if (PC->IsInputKeyDown(EKeys::D)) { Mv += Rgt; } if (PC->IsInputKeyDown(EKeys::A)) { Mv -= Rgt; }
			if (PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::SpaceBar)) { Mv += FVector::UpVector; } if (PC->IsInputKeyDown(EKeys::Q) || PC->IsInputKeyDown(EKeys::C)) { Mv -= FVector::UpVector; }
			ScoutPos += Mv * Sp;
			if (PC->IsInputKeyDown(EKeys::Z)) { ScoutFov -= 25.f * float(Real); } if (PC->IsInputKeyDown(EKeys::X)) { ScoutFov += 25.f * float(Real); } ScoutFov = FMath::Clamp(ScoutFov, 15.f, 110.f);
			ScriptNote = FString::Printf(TEXT("fly %.0f,%.0f,%.0f  p%.0f y%.0f  fov %.0f    WASD  Q/C down  E/Space up  shift fast  ctrl slow  wheel speed  Z X lens  F save"), ScoutPos.X, ScoutPos.Y, ScoutPos.Z, ScoutRot.Pitch, ScoutRot.Yaw, ScoutFov);
			if (PC->WasInputKeyJustPressed(EKeys::F)) { FString Old; const FString Path(TEXT("/Users/Shared/Epic Games/GekitotsuNative/Saved/fly.txt")); FFileHelper::LoadFileToString(Old, *Path); FFileHelper::SaveStringToFile(Old + ScriptNote + TEXT("\n"), *Path); }
		}
	}
	// ---- and the camera following it (camApply)
	{
		const float Rate = FMath::Clamp(Real * 60.f, 0.f, 4.f); auto Toward = [Rate](float k) { return 1.f - FMath::Pow(1.f - FMath::Clamp(k, 0.f, 0.95f), Rate); };
		if (CamD.bSnap) { CamD.Yaw = T.Yaw; CamD.Pit = T.Pit; CamD.Roll = T.Roll; CamD.Zm = T.Zm; CamD.Fov = T.Fov; CamD.bSnap = false; }
		CamD.Yaw += (T.Yaw - CamD.Yaw) * Toward(T.K); CamD.Pit += (T.Pit - CamD.Pit) * Toward(T.K); CamD.Roll += (T.Roll - CamD.Roll) * Toward(T.K);
		CamD.Zm += (T.Zm - CamD.Zm) * Toward(FMath::Max(0.1f, T.K)); CamD.Fov += (T.Fov - CamD.Fov) * Toward(FMath::Max(0.12f, T.K));
		CamD.KRoll *= FMath::Pow(0.86f, Rate); CamD.KZm *= FMath::Pow(0.84f, Rate); CamD.KFov *= FMath::Pow(0.86f, Rate); CamD.KYaw *= FMath::Pow(0.88f, Rate);
		// the browser's lens is far wider than this one, so its swings and widenings are scaled to suit
		const double Yaw = FMath::DegreesToRadians(double(CamD.Yaw + CamD.KYaw) * 0.8), Pit = FMath::DegreesToRadians(double(CamD.Pit) - 4.0);
		const double Fov = FMath::Clamp(GkFovBase + double(CamD.Fov + CamD.KFov) * 0.4, GkFovBase - 4.0, GkFovBase + 10.0), Hk = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5)), Hk0 = FMath::Tan(HalfFov);
		double D = -CamPos.X * (Hk0 / Hk) * double(CamD.Zm) * (1.0 + double(CamD.KZm));
		if (!T.bSolo) { const double A = (FMath::Abs(Ax - Bx) * 0.5 + 36.0 + T.Pad) * Unit; D = FMath::Max(D, A * (FMath::Cos(Yaw) / (0.84 * Hk) + FMath::Abs(FMath::Sin(Yaw)))); }
		double LookZ = CamPos.Z;
		if (bForestLook && bPlaced && Screen == 3)
		{
			const double GroundZ = 0.5 * (FloorPoint(At(S.p[0].x, 0)).Z + FloorPoint(At(S.p[1].x, 0)).Z);
			ForestCameraGround = FMath::FInterpTo(ForestCameraGround, GroundZ, Real, 5.0);
			LookZ += ForestCameraGround - 37.0;
		}
		if (bForestLook && Screen == 3) { D *= 1.08; }
		const FVector Look(0.0, CamPos.Y, LookZ); const double R = D * FMath::Cos(Pit);
		FVector Eye(-R * FMath::Cos(Yaw), Look.Y + R * FMath::Sin(Yaw), FMath::Max(14.0 + ForestCameraGround, Look.Z + D * FMath::Sin(Pit)));
		Eye += FVector(0.0, FMath::Sin(Ticks * 1.7) * Shake, FMath::Cos(Ticks * 2.3) * Shake * 0.7);
		FRotator Rot = (Look - Eye).Rotation(); Rot.Roll = -(CamD.Roll + CamD.KRoll) - (bForestLook && Screen == 3 ? 3.5 : 0.0);
		if (Cam) { Cam->GetCameraComponent()->SetFieldOfView(float(Fov)); Cam->SetActorLocationAndRotation(Wd(Eye), Anchor.GetRotation() * Rot.Quaternion()); }
		if (Cam)
		{   // the lens: the fighters are held sharp, the far ground goes soft behind them (not in the training room, where the grid is read)
			FPostProcessSettings& PP = Cam->GetCameraComponent()->PostProcessSettings; if (EdgeMid && !bEdgeAdd) { PP.AddBlendable(EdgeMid, 1.f); bEdgeAdd = true; } const bool bSoft = Place != 0 && LensF > 0.f && LensF < 20.f && !bScout;
			PP.bOverride_DepthOfFieldFstop = true; PP.DepthOfFieldFstop = bSoft ? LensF : 32.f; PP.bOverride_DepthOfFieldMinFstop = true; PP.DepthOfFieldMinFstop = 1.2f;
			PP.bOverride_DepthOfFieldSensorWidth = true; PP.DepthOfFieldSensorWidth = 36.f; PP.bOverride_DepthOfFieldFocalDistance = true; PP.DepthOfFieldFocalDistance = bSoft ? float((Look - Eye).Size()) : 0.f;
			if (bScout) { Cam->GetCameraComponent()->SetFieldOfView(ScoutFov); Cam->SetActorLocationAndRotation(ScoutPos, ScoutRot); }
		}
		Fx.Yaw = float(FMath::DegreesToRadians(Rot.Yaw));
	}

	// outside the editor the 3D picture is drawn no larger than 1080 lines, and smaller still if the machine cannot hold sixty
	if (!GIsEditor && GEngine && GEngine->GameViewport)
	{
		FVector2D Size; GEngine->GameViewport->GetViewportSize(Size); float Want2 = ScreenPct;
		if (ScreenPct <= 0.f && Size.Y > 0.0) { Want2 = FMath::Clamp(108000.f / float(Size.Y), 35.f, 100.f); }
		if (FrameMs > 19.f) { SlowSeconds += Real; } else { SlowSeconds = FMath::Max(0.f, SlowSeconds - Real * 0.5f); }
		const float MinPct = bForestLook ? 75.f : 35.f;
		if (SlowSeconds > 2.f && ScreenPct > MinPct) { Want2 = FMath::Max(MinPct, ScreenPct - 10.f); SlowSeconds = 0.f; }
		if (Want2 > 0.f && Want2 != ScreenPct)
		{
			ScreenPct = Want2;
			if (IConsoleVariable* Sp = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) { Sp->Set(ScreenPct, ECVF_SetByConsole); }
		}
	}
}
