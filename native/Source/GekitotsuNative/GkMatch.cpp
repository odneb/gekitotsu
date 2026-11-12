#include "GkMatch.h"
#include "GkPad.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/LightComponent.h"
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
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"

AGkMatch* AGkMatch::Current = nullptr;

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
	Restart();
	// -gkstatus=<file>: keep a short note of how the game is running in that file (the path may contain spaces)
	{ const FString Cmd = FCommandLine::Get(); const int32 From = Cmd.Find(TEXT("-gkstatus=")), To = Cmd.Find(TEXT(".txt"), ESearchCase::IgnoreCase, ESearchDir::FromStart, FMath::Max(0, From));
	  if (From >= 0 && To > From) { StatusPath = Cmd.Mid(From + 10, To + 4 - From - 10).Replace(TEXT("\""), TEXT("")); } }

	// a neutral place to fight: a floor, a marker at each end of the stage, daylight
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	TArray<TObjectPtr<USceneComponent>>* Into = &GridParts;
	auto Piece = [this, &Into](UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this); C->SetMobility(EComponentMobility::Movable); C->SetupAttachment(RootComponent);
		if (Mesh) { C->SetStaticMesh(Mesh); } C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent(); C->SetWorldLocation(Loc); C->SetWorldScale3D(Scale);
		if (Into) { Into->Add(C); }
		return C;
	};
	const double Half = double(gk::STAGE) * Cm;
	// A training room: a dark floor and back wall ruled with a measured grid, so every step, jump and knock-back can be
	// read against it. One square is 50 of the rules' units (about a metre); every second line is heavier. The gold line
	// is the middle of the stage and the red lines are its two ends.
	UMaterialInterface* Flat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto Paint = [Flat](UStaticMeshComponent* C, const FLinearColor& Col) { if (UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0, Flat)) { M->SetVectorParameterValue(TEXT("Color"), Col); } C->SetCastShadow(false); };
	const double WallX = 140.0, Sq = 50.0 * gk::F * Cm, Wide = 11.0 * Sq, High = 14.0 * Sq, Near = 700.0;
	const FLinearColor Slab(0.035f, 0.04f, 0.055f), Line(0.2f, 0.23f, 0.28f), Heavy(0.42f, 0.46f, 0.52f), Gold(0.9f, 0.6f, 0.1f), Red(0.8f, 0.1f, 0.08f);
	Paint(Piece(Plane, FVector(WallX - 1500.0, 0.0, -0.5), FVector(30.0, Wide * 2.0 / 100.0, 1.0)), Slab * 1.7f);
	{ UStaticMeshComponent* Wall = Piece(Plane, FVector(WallX, 0.0, High * 0.5), FVector(High / 100.0 + 20.0, Wide * 2.0 / 100.0, 1.0)); Wall->SetWorldRotation(FRotator(90.0, 0.0, 0.0)); Paint(Wall, Slab); }
	auto Upright = [&](double Y, double Thick, const FLinearColor& Col)
	{
		Paint(Piece(Cube, FVector(WallX - 2.0, Y, High * 0.5), FVector(0.02, Thick, High / 100.0)), Col);                    // up the wall
		Paint(Piece(Cube, FVector(WallX - Near * 0.5, Y, 0.0), FVector(Near / 100.0, Thick, 0.02)), Col);                   // and across the floor toward the camera
	};
	for (int32 k = -10; k <= 10; k++) { if (k != 0) { Upright(k * Sq, k % 2 == 0 ? 0.04 : 0.02, k % 2 == 0 ? Heavy : Line); } }
	Upright(0.0, 0.06, Gold); Upright(Half, 0.07, Red); Upright(-Half, 0.07, Red);
	for (int32 k = 1; k <= 13; k++) { Paint(Piece(Cube, FVector(WallX - 2.0, 0.0, k * Sq), FVector(0.02, Wide * 2.0 / 100.0, k % 2 == 0 ? 0.04 : 0.02)), k % 2 == 0 ? Heavy : Line); }
	for (int32 k = 0; k <= 7; k++) { Paint(Piece(Cube, FVector(WallX - k * Sq, 0.0, 0.0), FVector(k == 0 ? 0.06 : 0.02, Wide * 2.0 / 100.0, 0.02)), k == 0 ? Heavy : Line); }
	// The stage: a wasteland of red rock under a low sun. A sand floor, boulders close behind the fighters, mesas and
	// spires in the middle distance and great tablelands far off, each layer paler in the haze than the one before.
	Into = &StageParts;
	{
		UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
		auto Lit = [Flat](UStaticMeshComponent* C, const FLinearColor& Col, bool bShadow) { if (UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0, Flat)) { M->SetVectorParameterValue(TEXT("Color"), Col); } C->SetCastShadow(bShadow); };
		const FLinearColor Sand(0.52f, 0.31f, 0.16f), Rock(0.44f, 0.19f, 0.10f), Deep(0.27f, 0.11f, 0.07f);
		Lit(Piece(Plane, FVector(6000.0, 0.0, -0.5), FVector(400.0, 600.0, 1.0)), Sand, false);
		FRandomStream R(4411);
		auto Shade = [&R, Rock, Deep]() { return FMath::Lerp(Deep, Rock, R.FRandRange(0.35f, 1.f)); };
		auto Mesa = [&](double X, double Y, double W, double H, double D)
		{
			UStaticMeshComponent* Base = Piece(Cube, FVector(X, Y, H * 0.5), FVector(D / 100.0, W / 100.0, H / 100.0)); Base->SetWorldRotation(FRotator(0.0, R.FRandRange(-14.f, 14.f), R.FRandRange(-3.f, 3.f))); Lit(Base, Shade(), true);
			UStaticMeshComponent* Top = Piece(Cube, FVector(X, Y + W * R.FRandRange(-0.15f, 0.15f), H * 1.1), FVector(D * 0.8 / 100.0, W * R.FRandRange(0.5f, 0.8f) / 100.0, H * 0.28 / 100.0)); Top->SetWorldRotation(FRotator(0.0, R.FRandRange(-20.f, 20.f), 0.0)); Lit(Top, Shade(), true);
			Lit(Piece(Cone, FVector(X - D * 0.2, Y, H * 0.14), FVector(D * 1.7 / 100.0, W * 1.5 / 100.0, H * 0.3 / 100.0)), FMath::Lerp(Sand, Rock, 0.5f), false);
		};
		auto Spire = [&](double X, double Y, double Wide, double H)
		{
			UStaticMeshComponent* C = Piece(Cone, FVector(X, Y, H * 0.5), FVector(Wide / 100.0, Wide / 100.0, H / 100.0)); C->SetWorldRotation(FRotator(R.FRandRange(-4.f, 4.f), 0.0, R.FRandRange(-5.f, 5.f))); Lit(C, Shade(), true);
			Lit(Piece(Cone, FVector(X, Y, H * 0.1), FVector(Wide * 2.2 / 100.0, Wide * 2.2 / 100.0, H * 0.2 / 100.0)), FMath::Lerp(Sand, Rock, 0.5f), false);
		};
		for (int32 k = 0; k < 9; k++)   // boulders just behind the fight
		{
			const double Size = R.FRandRange(50.f, 150.f);
			UStaticMeshComponent* C = Piece(Cube, FVector(R.FRandRange(330.f, 900.f), R.FRandRange(-2200.f, 2200.f), Size * 0.3), FVector(Size / 100.0, Size * R.FRandRange(0.8f, 1.6f) / 100.0, Size * 0.8 / 100.0));
			C->SetWorldRotation(FRotator(R.FRandRange(-12.f, 12.f), R.FRandRange(0.f, 90.f), R.FRandRange(-12.f, 12.f))); Lit(C, Shade(), true);
		}
		for (int32 k = 0; k < 8; k++) { const double X = R.FRandRange(2200.f, 4600.f); if (k % 3 == 2) { Spire(X, R.FRandRange(-3600.f, 3600.f), R.FRandRange(140.f, 260.f), R.FRandRange(500.f, 1100.f)); } else { Mesa(X, R.FRandRange(-3800.f, 3800.f), R.FRandRange(400.f, 900.f), R.FRandRange(160.f, 420.f), R.FRandRange(400.f, 800.f)); } }
		for (int32 k = 0; k < 9; k++) { const double X = R.FRandRange(7000.f, 13000.f); if (k % 4 == 3) { Spire(X, R.FRandRange(-8000.f, 8000.f), R.FRandRange(400.f, 700.f), R.FRandRange(1500.f, 2600.f)); } else { Mesa(X, R.FRandRange(-9000.f, 9000.f), R.FRandRange(1500.f, 3200.f), R.FRandRange(500.f, 1300.f), R.FRandRange(1500.f, 2500.f)); } }
	}
	Into = nullptr;
	Fx.Init(this, RootComponent);
	// the low sun seen through the haze behind everything
	Fx.Add(0, FVector(14000.0, 2500.0, 700.0), FVector::ZeroVector, -1.f, 9000.f, 9000.f, FLinearColor(1.0f, 0.55f, 0.25f), 0.5f);
	for (int32 k = 0; k < 4; k++) { ShotMesh[k] = Piece(Ball, FVector::ZeroVector, FVector(0.45)); ShotMesh[k]->SetCastShadow(false); ShotMesh[k]->SetVisibility(false); }
	if (W)
	{
		if (ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector(0.0, 0.0, 800.0), FRotator(-31.0, 38.0, 0.0)))
		{
			if (ULightComponent* L = Sun->GetLightComponent()) { L->SetMobility(EComponentMobility::Movable); L->SetIntensity(8.f); L->SetLightColor(FLinearColor(1.f, 0.84f, 0.66f)); }
			if (UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(Sun->GetLightComponent())) { D->SetAtmosphereSunLight(true); D->SetDynamicShadowDistanceMovableLight(6000.f); }
		}
		Fog = W->SpawnActor<AExponentialHeightFog>(FVector(0.0, 0.0, 0.0), FRotator::ZeroRotator);
		if (Fog && Fog->GetComponent())
		{
			UExponentialHeightFogComponent* F = Fog->GetComponent(); F->SetMobility(EComponentMobility::Movable);
			F->SetFogDensity(0.022f); F->SetFogHeightFalloff(0.08f); F->SetFogInscatteringColor(FLinearColor(0.85f, 0.47f, 0.25f)); F->SetStartDistance(1200.f); F->SetFogMaxOpacity(0.92f);
		}
		USkyAtmosphereComponent* Air = NewObject<USkyAtmosphereComponent>(this); Air->SetupAttachment(RootComponent); Air->RegisterComponent();
		if (ASkyLight* Sky = W->SpawnActor<ASkyLight>(FVector(0.0, 0.0, 600.0), FRotator::ZeroRotator))
		{
			if (USkyLightComponent* L = Sky->GetLightComponent()) { L->SetMobility(EComponentMobility::Movable); L->SetRealTimeCapture(true); L->SetIntensity(1.f); }
		}
	}

	// The fighters. The skeletons are Unreal's mannequins until the real characters are made; the animation is the browser
	// game's own, sampled for every move and state (Content/Gk/poses.*) and solved onto the skeleton each frame.
	if (!Poses.Load(FPaths::ProjectContentDir() / TEXT("Gk"))) { UE_LOG(LogTemp, Error, TEXT("Gekitotsu: the pose data (Content/Gk/poses.txt, poses.bin) did not load")); }
	MoveClip.Reset(); for (int32 k = 0; k < gk::moveCount(); k++) { MoveClip.Add(FName(*(FString(TEXT("mv_")) + UTF8_TO_TCHAR(gk::moveKey(k))))); }
	static const TCHAR* Meshes[2] = { TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple") };
	for (int32 i = 0; i < 2; i++)
	{
		UPoseableMeshComponent* C = NewObject<UPoseableMeshComponent>(this); C->SetupAttachment(RootComponent);
		C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
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
			if (USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, Meshes[i])) { C->SetSkinnedAssetAndUpdate(M); }
			const TCHAR* Side = i ? TEXT("P2") : TEXT("P1"); bool bAnime = true;
			for (int32 k = 0; k < C->GetNumMaterials(); k++)
			{
				const FString Path = FString::Printf(TEXT("/Game/GkMat/MI_Gk_%s_%s.MI_Gk_%s_%s"), Side, k == 0 ? TEXT("A") : TEXT("B"), Side, k == 0 ? TEXT("A") : TEXT("B"));
				if (UMaterialInterface* Cel = LoadObject<UMaterialInterface>(nullptr, *Path)) { C->SetMaterial(k, Cel); }
				else
				{
					bAnime = false; UE_LOG(LogTemp, Warning, TEXT("Gekitotsu: the anime material %s did not load"), *Path);
					if (UMaterialInstanceDynamic* Tint = C->CreateDynamicMaterialInstance(k)) { Tint->SetVectorParameterValue(TEXT("Paint Tint"), i ? FLinearColor(0.05f, 0.22f, 0.95f) : FLinearColor(0.9f, 0.07f, 0.04f)); }
				}
			}
			if (bAnime) { if (UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GkMat/MI_Gk_Outline.MI_Gk_Outline"))) { C->SetOverlayMaterial(Ink); } }
		}
		C->SetBoundsScale(30.f); C->bNeverDistanceCull = true;
		Body[i] = C; Rig[i].Init(C);
	}
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
			  PP.bOverride_DepthOfFieldFstop = true; PP.DepthOfFieldFstop = 1.4f; PP.bOverride_DepthOfFieldFocalDistance = true; PP.DepthOfFieldFocalDistance = 900.f;
			  Cam->GetCameraComponent()->PostProcessBlendWeight = 1.f; }
			if (APlayerController* PC = W->GetFirstPlayerController()) { PC->SetViewTarget(Cam); }
		}
	}
}

void AGkMatch::EndPlay(const EEndPlayReason::Type Reason)
{
	delete Pads; Pads = nullptr; if (Current == this) { Current = nullptr; }
	Super::EndPlay(Reason);
}

void AGkMatch::Restart()
{
	gk::newMatch(S, Mode == 1, 2); gk::aiMake(Cpu[0], Level); gk::aiMake(Cpu[1], Level);
	OverTimer = 0; CallHold = 0; Hist.Reset(); LastIn = -1; Adv = -999; bExchange = false; Sparks.Reset();
	for (int32 i = 0; i < 2; i++) { Combo[i] = 0; ComboTimer[i] = 0; NoteTimer[i] = 0; ActFrame[i] = 0; bWasIdle[i] = true; }
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
	if (!bMenu)
	{
		if (Hit(EKeys::R) || Hit(EKeys::Gamepad_Special_Left) || bSelect) { Restart(); }
		if (Hit(EKeys::B)) { bBoxes = !bBoxes; }
		if (bToggle) { bMenu = true; MenuAt = 0; MenuPage = 0; }
		return;
	}
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
	if (bToggle) { bMenu = false; return; }
	const int32 Count = 7;
	if (Hit(EKeys::W) || Hit(EKeys::Up) || Hit(EKeys::Gamepad_DPad_Up) || (FreshDir & gk::IN_U)) { MenuAt = (MenuAt + Count - 1) % Count; }
	if (Hit(EKeys::S) || Hit(EKeys::Down) || Hit(EKeys::Gamepad_DPad_Down) || (FreshDir & gk::IN_D)) { MenuAt = (MenuAt + 1) % Count; }
	const bool bNext = Hit(EKeys::D) || Hit(EKeys::Right) || Hit(EKeys::Gamepad_DPad_Right) || (FreshDir & gk::IN_R);
	const bool bPrev = Hit(EKeys::A) || Hit(EKeys::Left) || Hit(EKeys::Gamepad_DPad_Left) || (FreshDir & gk::IN_L);
	const bool bOk = Hit(EKeys::Enter) || Hit(EKeys::J) || Hit(EKeys::Gamepad_FaceButton_Bottom) || Raw(0) || Raw(1);
	const int32 Way = bPrev ? -1 : ((bNext || bOk) ? 1 : 0);
	switch (MenuAt)
	{
		case 0: if (bOk) { bMenu = false; } break;
		case 1: if (Way != 0) { Mode = (Mode + Way + 3) % 3; Restart(); } break;
		case 2: if (Way != 0) { Level = 1 + (Level - 1 + Way + 3) % 3; gk::aiMake(Cpu[0], Level); gk::aiMake(Cpu[1], Level); } break;
		case 3: if (Way != 0) { Dummy = (Dummy + Way + 5) % 5; } break;
		case 4: if (Way != 0) { bBoxes = !bBoxes; } break;
		case 5: if (bOk) { MenuPage = 1; BindAt = 0; } break;
		default: if (bOk) { Restart(); bMenu = false; } break;
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
	InputNow = ReadPlayer();
	MenuInput();
	const double Step = 1.0 / 60.0, T0 = FPlatformTime::Seconds(); int32 Steps = 0;
	while (Acc >= Step && Steps < 8) { Acc -= Step; Steps++; Ticks++; if (!bMenu) { StepOnce(); } }
	if (Acc > Step) { Acc = 0.0; }
	StepMs = StepMs * 0.9f + float(FPlatformTime::Seconds() - T0) * 100.f;
	Present(float(Real));
	if (!StatusPath.IsEmpty() && (++StatusClock % 30) == 0)
	{
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("fps %d\n%s\nbuttons %08x dirs %d input %d\nframe %d menu %d\n"), FMath::RoundToInt(1000.f / FMath::Max(1.f, FrameMs)), Pads ? *Pads->Debug() : TEXT(""), PadNow, PadDirs, InputNow, S.frame, bMenu ? 1 : 0), *StatusPath);
	}
}

void AGkMatch::StepOnce()
{
	const int32 Human = InputNow; int32 I0 = Human, I1 = 0;
	if (Mode == 2) { I0 = gk::aiThink(Cpu[0], S, 0); I1 = gk::aiThink(Cpu[1], S, 1); }
	else if (Mode == 1) { I1 = DummyInput(); }
	else { I1 = gk::aiThink(Cpu[1], S, 1); }
	gk::step(S, I0, I1);
	for (int32 e = 0; e < S.nev; e++) { OnEvent(S.ev[e]); }
	if (Mode == 1) { Track(I0); }
	if (CallHold > 0 && ++CallAge > CallHold) { CallHold = 0; }
	for (int32 i = 0; i < 2; i++) { if (ComboTimer[i] > 0) { ComboTimer[i]--; } if (NoteTimer[i] > 0) { NoteTimer[i]--; } }
	for (int32 k = Sparks.Num() - 1; k >= 0; k--) { if (++Sparks[k].Age > 12) { Sparks.RemoveAtSwap(k); } }
	// the match is decided: any attack button, or a few seconds, starts the next
	if (S.phase == gk::PH_OVER) { if (++OverTimer > 420 || (OverTimer > 50 && (Human & 0xFF0))) { Restart(); } } else { OverTimer = 0; }
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

void AGkMatch::OnEvent(const gk::Event& E)
{
	const int32 Who = E.who >= 0 ? (E.who & 1) : 0;
	const FVector Feet = At(S.p[Who].x, S.p[Who].y); const FLinearColor Ki = Who ? FLinearColor(1.5f, 3.5f, 8.f) : FLinearColor(7.f, 3.6f, 0.9f);
	switch (E.type)
	{
		case gk::E_HIT:
			HitKind[1 - Who] = (E.f & gk::H_LOW) ? 3 : E.a >= 2 ? 2 : E.a >= 1 ? 1 : 0;   // how hard it landed picks the reel: light, medium, heavy, or swept low
			Fx.Hit(At(E.x, E.y), (E.f & gk::H_KO) ? 3 : FMath::Clamp(E.a, 0, 2), float(S.p[Who].face)); Shake = FMath::Max(Shake, 2.f + E.a * 2.f + ((E.f & gk::H_KO) ? 10.f : 0.f));
			Combo[Who] = E.c; ComboDmg[Who] = E.d; ComboTimer[Who] = 80;
			if (E.f & gk::H_IMPACT) { Remark(Who, (E.f & gk::H_CRUMPLE) ? TEXT("CRUMPLE!") : TEXT("BREACH!")); }
			else if (E.f & gk::H_FREE) { Remark(Who, TEXT("CHASE")); }
			else if (E.f & gk::H_LOW) { Remark(Who, TEXT("LOW!")); }
			else if (E.f & gk::H_OVERHEAD) { Remark(Who, TEXT("OVERHEAD!")); }
			else if (E.e == 2 && E.c <= 1) { Remark(Who, TEXT("PUNISH")); }
			else if (E.e == 1 && E.c <= 1) { Remark(Who, TEXT("COUNTER")); }
			break;
		case gk::E_BLOCK: Fx.Guard(At(E.x, E.y), float(S.p[Who].face)); Shake = FMath::Max(Shake, 1.5f); break;
		case gk::E_PARRY: Fx.Parry(At(E.x, E.y), false); break;
		case gk::E_PPARRY: Fx.Parry(At(E.x, E.y), true); Shake = FMath::Max(Shake, 5.f); Remark(Who, TEXT("PERFECT PARRY")); break;
		case gk::E_ARMOR: Fx.Guard(At(E.x, E.y), float(S.p[Who].face)); Remark(Who, TEXT("ARMOR")); break;
		case gk::E_DICLASH: Say(TEXT("CLASH!"), 50); Shake = FMath::Max(Shake, 12.f); break;
		case gk::E_SPLAT: Fx.Hit(Feet + FVector(0, 0, 100), 2, float(-S.p[Who].face)); Fx.Dust(Feet, 1.4f, 0.f); Remark(Who, E.a ? TEXT("STUN!") : TEXT("WALL SPLAT")); Shake = FMath::Max(Shake, 12.f); break;
		case gk::E_GBOUNCE: case gk::E_WBOUNCE: Fx.Dust(Feet, 1.6f, 0.f); Fx.Hit(FVector(Feet.X, Feet.Y, E.type == gk::E_GBOUNCE ? 10.0 : Feet.Z + 90.0), 1, 0.f); Shake = FMath::Max(Shake, 10.f); break;
		case gk::E_KD: Fx.Dust(Feet, 1.3f, 0.f); Shake = FMath::Max(Shake, 3.f); break;
		case gk::E_LAND: Fx.Dust(Feet, 0.55f, 0.f); break;
		case gk::E_JUMP: case gk::E_WALLJUMP: case gk::E_TECH: case gk::E_RISE: Fx.Dust(Feet, 0.45f, 0.f); break;
		case gk::E_DASH: case gk::E_RUN: Fx.Dust(Feet, 0.8f, float(S.p[Who].face)); break;
		case gk::E_EX: case gk::E_DI: case gk::E_HOME: Fx.Aura(Feet, Ki, 0.7f); break;
		case gk::E_RUSH: Fx.Aura(Feet, Ki, 0.9f); Remark(Who, TEXT("GHOST STEP")); break;
		case gk::E_BURNOUT: Remark(Who, TEXT("BURNOUT")); break;
		case gk::E_SUPER: Fx.Aura(Feet, Ki, 1.8f); Say(*FString(UTF8_TO_TCHAR(gk::moveName(E.c))).ToUpper(), 70); Shake = FMath::Max(Shake, 6.f); break;
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
/**
 * Shows one fighter on the frame the rules are on: picks the browser game's pose for his state or move at that frame
 * (the same choices the browser's fighterPose makes) and solves it onto the skeleton. Nothing plays by itself.
 */
void AGkMatch::ShowFighter(int32 i)
{
	UPoseableMeshComponent* C = Body[i]; if (!C) { return; }
	const gk::Fighter& P = S.p[i]; const gk::Fighter& O = S.p[1 - i];
	FName Clip(TEXT("idle")); int32 T = (S.frame + i * 44) % 262; float Roll = 0.f, Turn = 0.f;
	auto Ease = [](float K) { K = FMath::Clamp(K, 0.f, 1.f); return K * K * (3.f - 2.f * K); };
	switch (P.st)
	{
		case gk::IDLE:
			if (P.sub == gk::CROUCH) { Clip = TEXT("crouch"); T = FMath::Min(P.subf, 16); }
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
			if (MoveClip.IsValidIndex(P.mv)) { Clip = MoveClip[P.mv]; T = P.mf; }
			// a Slipstream that did not land cleanly loses its shape and tumbles
			if (M.has_home && P.mf >= M.home[0] && ((P.contact && !P.fr) || P.hmE)) { Clip = TEXT("ahit"); T = 0; Roll = -60.f - 30.f * FMath::Sin(P.mf * 0.5f); }
			break;
		}
		default: break;
	}
	const float* Pose = Poses.Frame(Clip, T); if (!Pose) { Pose = Poses.Frame(TEXT("idle"), 0); }
	const bool bLeft = P.face < 0;
	Rig[i].Apply(Pose, P.y > 0, bLeft, Roll, Turn);
	FVector Jit = FVector::ZeroVector;
	if (P.hs > 0 && (P.st == gk::HIT || P.st == gk::AHIT || P.st == gk::BLOCK)) { Jit.Y = ((Ticks & 1) ? 1.0 : -1.0) * FMath::Min(4.0, P.hs * 0.6); }
	// The skeleton faces its own +Y, which is the rules' "right"; the camera looks along +X. Facing left he is the mirrored
	// pose turned about, which shows the camera his chest exactly as the browser's mirror image does.
	C->SetWorldLocationAndRotation(At(P.x, P.y, i ? 8.0 : -8.0) + Jit, FRotator(0.0, bLeft ? 180.0 : 0.0, 0.0));
}

void AGkMatch::Present(float Real)
{
	for (int32 i = 0; i < 2; i++) { ShowFighter(i); }
	// which place is shown: training is fought in the measured room, everything else in the wasteland
	const int32 Place = Mode == 1 ? 0 : 1;
	if (Place != ShownPlace)
	{
		ShownPlace = Place;
		for (USceneComponent* C : GridParts) { if (C) { C->SetVisibility(Place == 0); } }
		for (USceneComponent* C : StageParts) { if (C) { C->SetVisibility(Place == 1); } }
		if (Fog && Fog->GetComponent()) { Fog->GetComponent()->SetVisibility(Place == 1); }
		Fx.DustCol = Place == 1 ? FLinearColor(0.62f, 0.47f, 0.33f) : FLinearColor(0.45f, 0.47f, 0.52f);
	}
	Fx.Air(CamPos.Y, Real, Place == 1); Fx.Tick(Real);
	for (int32 k = 0; k < 4; k++)
	{
		if (!ShotMesh[k]) { continue; }
		const bool bOn = k < S.nshots; ShotMesh[k]->SetVisibility(bOn); if (bOn) { ShotMesh[k]->SetWorldLocation(At(S.shot[k].x, S.shot[k].y)); if (!bMenu) { Fx.Trail(At(S.shot[k].x, S.shot[k].y), (S.shot[k].o & 1) ? FLinearColor(1.5f, 3.5f, 8.f) : FLinearColor(7.f, 3.6f, 0.9f)); } }
	}
	// the camera frames both fighters the way the browser does: wide enough for the two of them and for the higher one's head
	const double Ax = S.p[0].x / double(gk::F), Bx = S.p[1].x / double(gk::F), Hy = FMath::Max(S.p[0].y, S.p[1].y) / double(gk::F);
	const double Tw = FMath::Max3(236.0, FMath::Min(480.0, FMath::Abs(Ax - Bx) + 150.0), FMath::Min(600.0, (Hy + 108.0) * 2.09)), Ty = Tw * 9.0 / 32.0 - 17.0, Unit = Cm * gk::F;
	const double HalfFov = FMath::DegreesToRadians(15.0), Dist = (Tw * 0.5 * Unit) / FMath::Tan(HalfFov);
	const double Edge = FMath::Max(0.0, (gk::STAGE / double(gk::F) + 30.0) - Tw * 0.5), Tx = FMath::Clamp((Ax + Bx) * 0.5, -Edge, Edge);
	const FVector Want(-Dist, Tx * Unit, Ty * Unit);
	CamPos = FMath::Lerp(CamPos, Want, FMath::Clamp(double(Real) * 9.0, 0.0, 1.0));
	Shake *= FMath::Pow(0.85f, FMath::Clamp(Real * 60.f, 0.f, 4.f));
	if (Cam) { Cam->GetCameraComponent()->PostProcessSettings.DepthOfFieldFocalDistance = float(-CamPos.X); Cam->SetActorLocation(CamPos + FVector(0.0, FMath::Sin(Ticks * 1.7) * Shake, FMath::Cos(Ticks * 2.3) * Shake * 0.7)); }

	// outside the editor the 3D picture is drawn no larger than 1080 lines, and smaller still if the machine cannot hold sixty
	if (!GIsEditor && GEngine && GEngine->GameViewport)
	{
		FVector2D Size; GEngine->GameViewport->GetViewportSize(Size); float Want2 = ScreenPct;
		if (ScreenPct <= 0.f && Size.Y > 0.0) { Want2 = FMath::Clamp(108000.f / float(Size.Y), 35.f, 100.f); }
		if (FrameMs > 19.f) { SlowSeconds += Real; } else { SlowSeconds = FMath::Max(0.f, SlowSeconds - Real * 0.5f); }
		if (SlowSeconds > 2.f && ScreenPct > 35.f) { Want2 = FMath::Max(35.f, ScreenPct - 10.f); SlowSeconds = 0.f; }
		if (Want2 > 0.f && Want2 != ScreenPct)
		{
			ScreenPct = Want2;
			if (IConsoleVariable* Sp = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) { Sp->Set(ScreenPct, ECVF_SetByConsole); }
		}
	}
}
