#include "GkGame.h"
#include "GkMatch.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Animation/SkeletalMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"

FGkGame* FGkGame::Current = nullptr;
FGkGame::~FGkGame() { SetLumen(true); if (Current == this) { Current = nullptr; } }

/**
 * Lumen's bounced light and reflections cost about 20 ms a frame on a laptop, which a fighting game cannot spend.
 * A match runs without them, on screen-space reflections, and holds 60 frames a second; the editor gets them back after.
 */
void FGkGame::SetLumen(bool bOn)
{
	if (IConsoleVariable* Gi = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"))) { Gi->Set(bOn ? 1 : 0, ECVF_SetByConsole); }
	if (IConsoleVariable* Rf = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ReflectionMethod"))) { Rf->Set(bOn ? 1 : 2, ECVF_SetByConsole); }
}

static FLinearColor GkMix(const FLinearColor& A, const FLinearColor& B, float K) { return FLinearColor(A.R + (B.R - A.R) * K, A.G + (B.G - A.G) * K, A.B + (B.B - A.B) * K, 1.f); }
static float GkEase(float K) { K = FMath::Clamp(K, 0.f, 1.f); return K * K * (3.f - 2.f * K); }

static const TCHAR* GkMvKey(uint8 Mv)
{
	static const TCHAR* Keys[] = { TEXT("NONE"), TEXT("S5L"), TEXT("S5M"), TEXT("S5H"), TEXT("S2L"), TEXT("S2M"), TEXT("S2H"), TEXT("JL"), TEXT("JM"), TEXT("JH"), TEXT("J2H"),
		TEXT("FIRE"), TEXT("SURGE"), TEXT("RISE"), TEXT("VANISH"), TEXT("WHIRL"), TEXT("DIVE"), TEXT("HOME"), TEXT("THROW"), TEXT("SUPER1"), TEXT("SUPER2"), TEXT("SUPER3") };
	const int32 N = UE_ARRAY_COUNT(Keys);
	return Mv < N ? Keys[Mv] : Keys[0];
}

static UPoseableMeshComponent* GkMakeSkel(AActor* Owner, USkeletalMesh* Sk)
{
	UPoseableMeshComponent* C = NewObject<UPoseableMeshComponent>(Owner);
	C->SetupAttachment(Owner->GetRootComponent());
	C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
	C->RegisterComponent();
	if (Sk) { C->SetSkinnedAssetAndUpdate(Sk); }
	C->SetBoundsScale(8.f); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	return C;
}

static UStaticMeshComponent* GkMakeQuad(AActor* Owner, UStaticMesh* Plane, UMaterialInterface* Mat, UMaterialInstanceDynamic*& OutMat)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Owner->GetRootComponent());
	C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
	if (Plane) { C->SetStaticMesh(Plane); }
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
	C->RegisterComponent();
	OutMat = Mat ? C->CreateDynamicMaterialInstance(0, Mat) : nullptr;
	C->SetVisibility(false);
	return C;
}

static UPointLightComponent* GkMakeLight(AActor* Owner)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(Owner->GetRootComponent());
	L->SetUsingAbsoluteLocation(true);
	L->RegisterComponent();
	L->SetIntensityUnits(ELightUnits::Candelas); L->SetAttenuationRadius(1100.f); L->SetCastShadows(false); L->SetIntensity(0.f);
	return L;
}

static void GkSetQuad(UStaticMeshComponent* Quad, const FVector& Pos, float RotDeg, float Tall, float Wide)
{
	// the plane lies flat; stood up to face the camera, its local X runs up the screen and its local Y along the line of play
	const FQuat Q = FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(RotDeg)) * FRotator(90.0, 0.0, 0.0).Quaternion();
	Quad->SetWorldLocationAndRotation(Pos, Q); Quad->SetWorldScale3D(FVector(Tall * 0.01f, Wide * 0.01f, 1.f)); Quad->SetVisibility(true);
}

FLinearColor FGkGame::Hue(int32 Who) const { return Who ? FLinearColor(0.7f, 2.4f, 7.f) : FLinearColor(7.f, 1.1f, 0.22f); }
FLinearColor FGkGame::HueCore(int32 Who) const { return Who ? FLinearColor(6.f, 8.f, 9.f) : FLinearColor(9.f, 7.f, 3.6f); }

// ------------------------------------------------------------------------------------------------ setting up
void FGkGame::Begin(AGkMatch* InOwner)
{
	Owner = InOwner; World = Owner->GetWorld(); Current = this; gk::init(S); Fx.SetNum(56);
	FString Err; if (!GkLib().Load(FPaths::ProjectContentDir() / TEXT("Gk/kagu.txt"), Err)) { UE_LOG(LogTemp, Warning, TEXT("Gekitotsu: %s"), *Err); }
	if (!GkAnim().Load(FPaths::ProjectContentDir() / TEXT("Gk"), Err)) { UE_LOG(LogTemp, Warning, TEXT("Gekitotsu: %s"), *Err); }
	Bind[0] = EKeys::Gamepad_FaceButton_Left; Bind[1] = EKeys::Gamepad_FaceButton_Top; Bind[2] = EKeys::Gamepad_RightShoulder; Bind[3] = EKeys::Gamepad_LeftShoulder;
	Bind[4] = EKeys::Gamepad_FaceButton_Bottom; Bind[5] = EKeys::Gamepad_FaceButton_Right; Bind[6] = EKeys::Gamepad_RightTrigger; Bind[7] = EKeys::Gamepad_LeftTrigger;
	LoadBinds();
	if (!World) { return; }
	SetLumen(false);
	// the figures placed in the level are for looking at in the editor; the match brings its own
	for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It) { It->SetActorHiddenInGame(true); }
	SkMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Kagu2/Kagu_V2.Kagu_V2"));
	if (SkMesh) { Rig.Init(SkMesh->GetRefSkeleton()); }
	InkMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mat/MI_Ink.MI_Ink"));
	UMaterialInterface* GhostM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mat/M_Ghost.M_Ghost"));
	UMaterialInterface* FxM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mat/M_Fx.M_Fx"));
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	for (int32 i = 0; i < 2; i++)
	{
		FGkView& Vw = V[i];
		Vw.Mesh = GkMakeSkel(Owner, SkMesh);
		UMaterialInterface* Body = LoadObject<UMaterialInterface>(nullptr, i ? TEXT("/Game/Mat/MI_Shiro.MI_Shiro") : TEXT("/Game/Mat/MI_Kagu.MI_Kagu"));
		if (Body)
		{
			Vw.BodyMat = Vw.Mesh->CreateDynamicMaterialInstance(0, Body);
			for (int32 m = 1; m < Vw.Mesh->GetNumMaterials(); m++) { Vw.Mesh->SetMaterial(m, Vw.BodyMat); }
		}
		if (InkMat) { Vw.Mesh->SetOverlayMaterial(InkMat); }
		Vw.Mesh->SetCastShadow(true);
		for (int32 g = 0; g < 4; g++)
		{
			Vw.Ghost[g] = GkMakeSkel(Owner, SkMesh); Vw.Ghost[g]->SetCastShadow(false); Vw.Ghost[g]->SetVisibility(false);
			if (GhostM)
			{
				Vw.GhostMat[g] = Vw.Ghost[g]->CreateDynamicMaterialInstance(0, GhostM);
				for (int32 m = 1; m < Vw.Ghost[g]->GetNumMaterials(); m++) { Vw.Ghost[g]->SetMaterial(m, Vw.GhostMat[g]); }
			}
		}
	}
	for (FGkFx& F : Fx) { F.Quad = GkMakeQuad(Owner, Plane, FxM, F.Mat); }
	for (int32 i = 0; i < 2; i++)
	{
		for (int32 k = 0; k < 2; k++) { ShotQuad[i][k] = GkMakeQuad(Owner, Plane, FxM, ShotMat[i][k]); }
		ShotLight[i] = GkMakeLight(Owner);
	}
	for (int32 k = 0; k < 3; k++) { SheetQuad[k] = GkMakeQuad(Owner, Plane, FxM, SheetMat[k]); }
	HitLight = GkMakeLight(Owner);

	// the parts of the world that answer to the fight
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		ULightComponent* L = It->GetLightComponent(); if (!L) { continue; }
		if (L->Intensity > 1.5f) { Sun = L; } else { Fill = L; }
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It) { Sky = It->GetLightComponent(); }
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It) { Fog = It->GetComponent(); }
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		UStaticMeshComponent* C = It->GetStaticMeshComponent(); if (!C) { continue; }
		if (It->ActorHasTag(TEXT("Sky"))) { SkyMat = C->CreateDynamicMaterialInstance(0); }
		if (It->ActorHasTag(TEXT("SunDisc"))) { SunMat = C->CreateDynamicMaterialInstance(0); }
	}
	Cam = World->SpawnActor<ACameraActor>(CamPos, FRotator(0, 180, 0));
	if (Cam)
	{
		Cam->GetCameraComponent()->SetFieldOfView(42.f); Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
		if (APlayerController* PC = World->GetFirstPlayerController()) { PC->SetViewTarget(Cam); }
	}
}

// ------------------------------------------------------------------------------------------------ controls
uint16 FGkGame::ReadPlayer() const
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr; if (!PC) { return 0; }
	auto K = [PC](const FKey& Key) { return PC->IsInputKeyDown(Key); };
	uint16 In = 0;
	const float SX = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX), SY = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	if (K(EKeys::A) || K(EKeys::Left) || K(EKeys::Gamepad_DPad_Left) || SX < -0.5f) { In |= gk::LEFT; }
	if (K(EKeys::D) || K(EKeys::Right) || K(EKeys::Gamepad_DPad_Right) || SX > 0.5f) { In |= gk::RIGHT; }
	if (K(EKeys::W) || K(EKeys::Up) || K(EKeys::SpaceBar) || K(EKeys::Gamepad_DPad_Up) || SY > 0.5f) { In |= gk::UP; }
	if (K(EKeys::S) || K(EKeys::Down) || K(EKeys::Gamepad_DPad_Down) || SY < -0.5f) { In |= gk::DOWN; }
	// Light, Medium, Heavy, Special, Parry, Throw, Slipstream, Super: fixed keys, plus the pad buttons the player has bound
	static const FKey Keys[8] = { EKeys::J, EKeys::K, EKeys::L, EKeys::I, EKeys::U, EKeys::O, EKeys::H, EKeys::P };
	static const uint16 Bits[8] = { gk::BL, gk::BM, gk::BH, gk::BS, gk::BP, gk::BL | gk::BM, gk::BM | gk::BH, gk::BH | gk::BS };
	for (int32 a = 0; a < 8; a++) { if (K(Keys[a]) || (Bind[a].IsValid() && K(Bind[a]))) { In |= Bits[a]; } }
	return In;
}

/** A sparring partner. It only reads the same state a player sees and presses the same buttons. */
uint16 FGkGame::Cpu(int32 Me)
{
	const gk::Player& P = S.p[Me]; const gk::Player& O = S.p[1 - Me];
	Rng ^= Rng << 13; Rng ^= Rng >> 17; Rng ^= Rng << 5; const uint32 R = Rng % 1000;
	const int32 Dist = FMath::Abs(O.x - P.x) / gk::F; const uint16 Fwd = P.face > 0 ? gk::RIGHT : gk::LEFT, Back = P.face > 0 ? gk::LEFT : gk::RIGHT;
	const bool bTick = (S.frame % 2) == 0;   // buttons are tapped, not held
	if (S.phase != gk::PH_FIGHT) { return 0; }
	if (P.st == gk::LAUNCH) { return (P.stun == 0 && bTick) ? gk::BL : 0; }
	if (P.st == gk::ATK && P.connected == 1)
	{
		// it landed: keep the combo going
		if (!bTick) { return 0; }
		if (P.mv == gk::S2H) { return gk::UP; }
		if (P.y > 0) { return P.mv == gk::JL ? gk::BM : P.mv == gk::JM ? gk::BH : (R < 500 ? uint16(gk::BM | gk::BH) : uint16(gk::BS | gk::DOWN)); }
		return P.mv == gk::S5L ? gk::BM : P.mv == gk::S5M ? gk::BH : P.mv == gk::S5H ? uint16(gk::BH | gk::DOWN) : (R < 400 ? uint16(gk::BM | gk::BH) : 0);
	}
	if (O.st == gk::LAUNCH && O.y > 60 * gk::F)
	{
		if (P.y > 0) { return bTick ? (Dist < 130 ? (R < 500 ? gk::BL : gk::BH) : uint16(gk::BM | gk::BH)) : 0; }
		return R < 60 ? uint16(gk::BM | gk::BH) : (Dist < 200 ? uint16(gk::UP | Fwd) : Fwd);
	}
	if (O.st == gk::ATK && Dist < 260 && R < 720) { return Back | ((gk::move(O.mv).flags & gk::LOW) ? gk::DOWN : 0); }
	if (S.shot[1 - Me].on && R < 600) { return Back; }
	if (Dist > 420) { return R < 14 ? uint16(gk::BM | gk::BH) : R < 26 ? gk::BS : R < 40 ? uint16(gk::UP | Fwd) : Fwd; }
	if (Dist > 150) { return R < 12 ? uint16(gk::BS | Fwd) : R < 22 ? uint16(gk::BM | gk::BH) : R < 34 ? uint16(gk::UP | Fwd) : R < 44 ? uint16(gk::BS | Back) : Fwd; }
	if (!bTick) { return 0; }
	if (P.y > 0) { return R < 300 ? gk::BH : R < 500 ? gk::BM : 0; }
	return R < 150 ? gk::BL : R < 280 ? gk::BM : R < 380 ? uint16(gk::BM | gk::DOWN) : R < 450 ? gk::BH : R < 500 ? uint16(gk::BH | gk::DOWN) : R < 550 ? uint16(gk::BL | gk::BM) : R < 590 ? uint16(gk::BS | gk::DOWN) : R < 620 ? gk::BP : R < 640 ? uint16(gk::BH | gk::BS) : R < 700 ? Back : 0;
}

// ------------------------------------------------------------------------------------------------ modes, menu, buttons
void FGkGame::Restart() { gk::init(S); OverTimer = 0; Combo[0] = Combo[1] = 0; ComboTimer[0] = ComboTimer[1] = 0; }

/** What the training dummy does: stand, crouch, jump, block what is thrown at it, or fight back. */
uint16 FGkGame::DummyInput()
{
	const gk::Player& D = S.p[1]; const gk::Player& A = S.p[0]; const uint16 Away = D.face > 0 ? gk::LEFT : gk::RIGHT;
	switch (Dummy)
	{
		case 1: return gk::DOWN;
		case 2: return (S.frame % 50) < 3 ? gk::UP : uint16(0);
		case 3: return (A.st == gk::ATK || S.shot[0].on) ? Away : uint16(0);
		case 4: return Cpu(1);
		default: return 0;
	}
}

void FGkGame::SaveBinds() const
{
	FString Out; for (int32 a = 0; a < 8; a++) { Out += Bind[a].GetFName().ToString() + TEXT("\n"); }
	FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("gk_binds.txt")));
}

void FGkGame::LoadBinds()
{
	FString Text; if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("gk_binds.txt")))) { return; }
	TArray<FString> Lines; Text.ParseIntoArrayLines(Lines);
	for (int32 a = 0; a < FMath::Min(8, Lines.Num()); a++) { const FKey Key = FKey(FName(*Lines[a])); if (Key.IsValid()) { Bind[a] = Key; } }
}

/** The menu is drawn by the HUD; this reads it. TAB or START opens it, and the match waits while it is open. */
void FGkGame::MenuInput()
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr; if (!PC) { return; }
	auto Hit = [PC](const FKey& Key) { return PC->WasInputKeyJustPressed(Key); };
	const bool bToggle = Hit(EKeys::Tab) || Hit(EKeys::Gamepad_Special_Right);
	if (!bMenu)
	{
		if (Hit(EKeys::One)) { Mode = 1; Restart(); }
		if (Hit(EKeys::Two)) { Mode = 2; Restart(); }
		if (Hit(EKeys::Three)) { Mode = 0; Restart(); }
		if (Hit(EKeys::R) || Hit(EKeys::Gamepad_Special_Left)) { Restart(); }
		if (bToggle) { bMenu = true; MenuAt = 0; MenuPage = 0; }
		return;
	}
	if (MenuPage == 1)
	{
		// the next pad button pressed becomes the action being asked for
		static const FKey Pad[] = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder,
			EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_RightTrigger, EKeys::Gamepad_LeftThumbstick, EKeys::Gamepad_RightThumbstick, EKeys::Gamepad_Special_Left };
		if (bToggle) { MenuPage = 0; return; }
		for (const FKey& Key : Pad)
		{
			if (!Hit(Key)) { continue; }
			Bind[BindAt] = Key; if (++BindAt >= 8) { BindAt = 0; MenuPage = 0; SaveBinds(); }
			break;
		}
		return;
	}
	if (bToggle) { bMenu = false; return; }
	const int32 Count = 5;
	if (Hit(EKeys::W) || Hit(EKeys::Up) || Hit(EKeys::Gamepad_DPad_Up) || Hit(EKeys::Gamepad_LeftStick_Up)) { MenuAt = (MenuAt + Count - 1) % Count; }
	if (Hit(EKeys::S) || Hit(EKeys::Down) || Hit(EKeys::Gamepad_DPad_Down) || Hit(EKeys::Gamepad_LeftStick_Down)) { MenuAt = (MenuAt + 1) % Count; }
	const bool bNext = Hit(EKeys::D) || Hit(EKeys::Right) || Hit(EKeys::Gamepad_DPad_Right) || Hit(EKeys::Gamepad_LeftStick_Right);
	const bool bPrev = Hit(EKeys::A) || Hit(EKeys::Left) || Hit(EKeys::Gamepad_DPad_Left) || Hit(EKeys::Gamepad_LeftStick_Left);
	const bool bOk = Hit(EKeys::Enter) || Hit(EKeys::J) || Hit(EKeys::SpaceBar) || Hit(EKeys::Gamepad_FaceButton_Bottom);
	const int32 Way = bPrev ? -1 : ((bNext || bOk) ? 1 : 0);
	switch (MenuAt)
	{
		case 0: if (bOk) { bMenu = false; } break;
		case 1: if (Way != 0) { Mode = (Mode + Way + 3) % 3; Restart(); } break;
		case 2: if (Way != 0) { Dummy = (Dummy + Way + 5) % 5; } break;
		case 3: if (bOk) { MenuPage = 1; BindAt = 0; } break;
		default: if (bOk) { Restart(); bMenu = false; } break;
	}
}

// ------------------------------------------------------------------------------------------------ running the match
void FGkGame::Tick(float DeltaSeconds)
{
	// the match keeps real time even when the picture cannot: a slow frame drops pictures, never speed
	const double Now = FPlatformTime::Seconds(); const double Real = LastReal > 0.0 ? FMath::Clamp(Now - LastReal, 0.0, 0.25) : double(DeltaSeconds); LastReal = Now;
	Acc += Real; FrameMs = FrameMs * 0.9f + float(Real) * 100.f;
	const double Step = 1.0 / 60.0, T0 = FPlatformTime::Seconds();
	MenuInput();
	int32 Steps = 0;
	while (Acc >= Step && Steps < 12)
	{
		Acc -= Step; Tick60++; Steps++;
		bool bRun = !bPaused && !bMenu;
		if (bPaused && StepsOwed > 0) { StepsOwed--; bRun = true; }
		// the last blow of a round plays out slowly
		if (bRun && SlowFrames > 0) { SlowFrames--; if (SlowFrames % 3 != 0) { bRun = false; } }
		if (bRun) { StepOnce(); }
		if (Mono > 0) { Mono--; }
		if (SuperTimer > 0) { SuperTimer--; }
	}
	if (Acc > Step) { Acc = 0.0; }
	// the simulation may step several times between pictures; the picture is brought up to date once
	const double T1 = FPlatformTime::Seconds();
	if (Steps > 0) { TickFx(Steps); for (int32 i = 0; i < 2; i++) { ShowFighter(i, Steps); } }
	ShowWorld(float(FMath::Min(Real, 0.1)));
	const double T2 = FPlatformTime::Seconds();
	StepMs = StepMs * 0.9f + float(T1 - T0) * 100.f; ShowMs = ShowMs * 0.9f + float(T2 - T1) * 100.f;
}

void FGkGame::StepOnce()
{
	const uint16 Human = ReadPlayer();
	IdleSeconds = Human ? 0.0 : IdleSeconds + 1.0 / 60.0;
	// nobody at the controls for a while: the sparring partner takes both sides, so the game demonstrates itself
	uint16 In[2];
	In[0] = Mode == 0 ? Cpu(0) : Human;      // player one is always the person at the controls, unless the mode is to watch
	In[1] = Mode == 2 ? DummyInput() : (bCpu[1] ? Cpu(1) : uint16(0));
	LastIn = Human;
	for (int32 i = 0; i < 2; i++) { if (ScriptFrames[i] > 0) { ScriptFrames[i]--; In[i] = uint16(ScriptIn[i]); } }
	gk::step(S, In[0], In[1]);
	for (int32 e = 0; e < S.nev; e++) { OnEvent(S.ev[e]); }
	if (Mode == 2)
	{
		// training: the clock never runs out, you never tire, and the dummy recovers as soon as a combo ends
		S.time = gk::ROUND_TIME; S.p[0].hp = gk::MAX_HP; S.p[0].meter = gk::MAX_METER; S.p[1].meter = gk::MAX_METER;
		if ((S.p[1].st == gk::IDLE || S.p[1].st == gk::BLOCK) && S.p[1].stun == 0 && S.p[1].hp > 0) { S.p[1].hp = gk::MAX_HP; }
	}
	for (int32 i = 0; i < 2; i++)
	{
		const gk::Player& P = S.p[i];
		if (ComboTimer[i] > 0) { ComboTimer[i]--; }
		if (ComboPop[i] > 0) { ComboPop[i]--; }
		if (V[i].Flash > 0) { V[i].Flash--; }
		if (P.st != gk::ATK) { PrevMv[i] = 0; PrevMf[i] = 0; continue; }
		const gk::Move& M = gk::move(P.mv); const FGkMoveAnim& A = GkLib().Move(FName(GkMvKey(P.mv)));
		const bool bFresh = PrevMv[i] != P.mv || PrevMf[i] < M.su + 1 || PrevMf[i] > P.mf;
		if (P.mf >= M.su + 1 && bFresh && A.ArcSize > 0.f && M.hb.w > 0)
		{
			// the swing trail: drawn once, where the blow lands
			const int32 Cx = P.x + P.face * (M.hb.x + M.hb.w / 2) * gk::F, Cy = P.y + (M.hb.y + M.hb.h / 2) * gk::F;
			FGkFx& F = Spawn(A.ArcKind, At(Cx, Cy, 26.0), A.ArcSize * 0.8f, A.ArcKind == 3 ? 7.f : 10.f, Hue(i), HueCore(i));
			F.Size1 = A.ArcSize * 1.12f; F.Rot = P.face > 0 ? A.Arc : 180.f - A.Arc;
			if (A.ArcKind == 3) { F.Stretch = 3.4f; F.Size0 = A.ArcSize * 0.3f; F.Size1 = A.ArcSize * 0.36f; }
		}
		if (P.mv == gk::HOME && P.mf > M.su && (S.frame & 1) == 0)
		{
			// Slipstream leaves a wake
			FGkFx& F = Spawn(3, At(P.x, P.y + 95 * gk::F, -14.0), 46.f, 10.f, Hue(i), HueCore(i));
			F.Size1 = 20.f; F.Stretch = 7.f; F.Alpha = 0.75f; F.Rot = FMath::RadiansToDegrees(FMath::Atan2(float(P.vy), float(P.vx)));
		}
		PrevMv[i] = P.mv; PrevMf[i] = P.mf;
	}
	if (S.phase == gk::PH_OVER) { if (++OverTimer > 300) { OverTimer = 0; gk::init(S); } }
	else { OverTimer = 0; }
}

void FGkGame::OnEvent(const gk::Event& E)
{
	const int32 Who = E.who & 1; const FVector At0 = At(E.x, E.y);
	const FLinearColor Pale(5.f, 6.f, 7.f), Dust(0.5f, 0.42f, 0.5f), InkCol(0.02f, 0.005f, 0.03f);
	auto Burst = [this](const FVector& Where, float Size, const FLinearColor& A, const FLinearColor& B)
	{
		FGkFx& St = Spawn(1, Where, Size * 0.7f, 9.f, A, B); St.Size1 = Size * 1.3f; St.Rot = Rand() * 360.f;
		FGkFx& Rg = Spawn(2, Where, Size * 0.5f, 11.f, B, B); Rg.Size1 = Size * 1.7f;
		for (int32 k = 0; k < 4; k++)
		{
			const float Ang = Rand() * 360.f, Rad = FMath::DegreesToRadians(Ang);
			FGkFx& Ln = Spawn(3, Where + FVector(0.0, -FMath::Cos(Rad) * Size * 0.5, FMath::Sin(Rad) * Size * 0.5), Size * 0.08f, 9.f, B, B);
			Ln.Stretch = 5.f; Ln.Rot = Ang; Ln.Vel = FVector(0.0, -FMath::Cos(Rad) * Size * 0.07, FMath::Sin(Rad) * Size * 0.07); Ln.Size1 = Size * 0.04f;
		}
	};
	auto Puff = [this, &Dust](int32 X, int32 Y, float Size)
	{
		for (int32 k = -1; k <= 1; k += 2)
		{
			FGkFx& D = Spawn(0, At(X, Y + 8 * gk::F, 12.0), Size, 22.f, Dust, Dust);
			D.Size1 = Size * 2.2f; D.Alpha = 0.34f; D.Vel = FVector(0.0, k * 2.4, 0.5);
		}
	};
	auto Glow = [this](const FVector& Where, const FLinearColor& C, float Amount) { HitAt = Where + FVector(60.0, 0.0, 0.0); HitCol = C; HitGlow = FMath::Max(HitGlow, Amount); };
	switch (E.type)
	{
		case gk::E_HIT:
		{
			const int32 Level = E.a;
			Burst(At0, 110.f + Level * 46.f, Hue(Who), HueCore(Who)); Glow(At0, Hue(Who), 0.6f + Level * 0.3f);
			Shake = FMath::Max(Shake, 2.f + Level * 1.8f); Combo[Who] = E.b; ComboTimer[Who] = 80; ComboPop[Who] = 8; V[1 - Who].Flash = 3;
			if (Level >= 3) { Punch = FMath::Max(Punch, 1.f); Mono = FMath::Max(Mono, 3); if (Storm > 0.5f) { Flash = 1.f; } }
			break;
		}
		case gk::E_BLOCK: { FGkFx& Rg = Spawn(2, At0, 50.f, 12.f, Pale, Pale); Rg.Size1 = 170.f; FGkFx& St = Spawn(1, At0, 50.f, 8.f, Hue(1), Pale); St.Size1 = 90.f; Shake = FMath::Max(Shake, 1.5f); Glow(At0, Pale, 0.4f); break; }
		case gk::E_PARRY: { Burst(At0, 240.f, FLinearColor(1.f, 6.f, 8.f), FLinearColor(9.f, 10.f, 10.f)); Mono = FMath::Max(Mono, 7); White = FMath::Max(White, 0.5f); Shake = FMath::Max(Shake, 6.f); Glow(At0, Pale, 1.6f); break; }
		case gk::E_JUMP: case gk::E_LAND: case gk::E_DOWN: Puff(E.x, 0, E.type == gk::E_DOWN ? 90.f : 60.f); if (E.type == gk::E_DOWN) { Shake = FMath::Max(Shake, 3.f); } break;
		case gk::E_DASH: Puff(E.x, 0, 70.f); break;
		case gk::E_DJUMP: { FGkFx& Rg = Spawn(2, At(E.x, E.y), 60.f, 12.f, Pale, Pale); Rg.Size1 = 60.f; Rg.Stretch = 3.2f; Rg.Alpha = 0.8f; break; }
		case gk::E_HOME:
		{
			FGkFx& Rg = Spawn(2, At(E.x, E.y + 90 * gk::F), 80.f, 14.f, Hue(Who), HueCore(Who)); Rg.Size1 = 300.f;
			FGkFx& Ln = Spawn(8, At(E.x, E.y + 90 * gk::F, 40.0), 700.f, 16.f, HueCore(Who), HueCore(Who)); Ln.Size1 = 900.f; Ln.Alpha = 0.55f;
			Punch = FMath::Max(Punch, 0.6f); break;
		}
		case gk::E_HOMEHIT: Burst(At0, 250.f, Hue(Who), HueCore(Who)); Mono = FMath::Max(Mono, 3); Punch = 1.f; Shake = FMath::Max(Shake, 8.f); break;
		case gk::E_WALLB: { FGkFx& Rg = Spawn(2, At0, 120.f, 18.f, Pale, Pale); Rg.Size1 = 520.f; Rg.Stretch = 0.35f; Burst(At0, 200.f, Pale, Pale); Shake = FMath::Max(Shake, 13.f); Mono = FMath::Max(Mono, 4); Flash = FMath::Max(Flash, Storm); break; }
		case gk::E_GROUNDB: { FGkFx& Rg = Spawn(2, At(E.x, 6 * gk::F), 90.f, 18.f, Pale, Pale); Rg.Size1 = 200.f; Rg.Stretch = 4.f; Puff(E.x, 0, 120.f); Burst(At(E.x, 20 * gk::F), 180.f, Pale, Pale); Shake = FMath::Max(Shake, 13.f); Mono = FMath::Max(Mono, 4); break; }
		case gk::E_SUPER:
		{
			SuperWho = Who; SuperLevel = E.a + (E.b ? 1 : 0); SuperTimer = 100;
			FGkFx& Ln = Spawn(8, At(E.x, E.y + 100 * gk::F, 44.0), 900.f, 44.f, HueCore(Who), HueCore(Who)); Ln.Size1 = 1500.f;
			FGkFx& St = Spawn(1, At(E.x, E.y + 100 * gk::F, 40.0), 200.f, 26.f, Hue(Who), HueCore(Who)); St.Size1 = 520.f;
			White = FMath::Max(White, 0.7f); Shake = FMath::Max(Shake, 7.f); Glow(At(E.x, E.y + 100 * gk::F), Hue(Who), 3.f);
			break;
		}
		case gk::E_KO: SlowFrames = 96; White = 1.f; Mono = FMath::Max(Mono, 34); Shake = FMath::Max(Shake, 16.f); Punch = 1.4f; Flash = 1.f; break;
		case gk::E_VANISH: { FGkFx& St = Spawn(1, At(E.x, E.y + 90 * gk::F), 150.f, 14.f, InkCol, Hue(Who)); St.Size1 = 260.f; St.Rot = Rand() * 360.f; break; }
		case gk::E_FIRE: { FGkFx& St = Spawn(1, At0, 70.f, 8.f, Hue(Who), HueCore(Who)); St.Size1 = 150.f; Glow(At0, Hue(Who), 0.8f); break; }
		case gk::E_THROW: Burst(At(E.x, E.y + 100 * gk::F), 190.f, Hue(Who), HueCore(Who)); Shake = FMath::Max(Shake, 8.f); break;
		case gk::E_THROWTECH: Burst(At0, 150.f, Pale, Pale); break;
		case gk::E_TECH: { FGkFx& Rg = Spawn(2, At(E.x, E.y + 80 * gk::F), 60.f, 12.f, Pale, Pale); Rg.Size1 = 200.f; break; }
		default: break;
	}
}

// ------------------------------------------------------------------------------------------------ effects
FGkFx& FGkGame::Spawn(int32 Kind, const FVector& Pos, float Size, float Life, const FLinearColor& Col, const FLinearColor& Core)
{
	FGkFx* Pick = &Fx[0]; float Oldest = -1.f;
	for (FGkFx& F : Fx)
	{
		if (!F.bOn) { Pick = &F; break; }
		const float K = F.Age / F.Life; if (K > Oldest) { Oldest = K; Pick = &F; }
	}
	UStaticMeshComponent* Q = Pick->Quad; UMaterialInstanceDynamic* M = Pick->Mat;
	*Pick = FGkFx(); Pick->Quad = Q; Pick->Mat = M; Pick->bOn = true; Pick->Kind = Kind; Pick->Pos = Pos; Pick->Size0 = Size; Pick->Size1 = Size * 1.5f;
	Pick->Life = FMath::Max(1.f, Life); Pick->Col = Col; Pick->Core = Core; Pick->Seed = Rand();
	return *Pick;
}

static void GkFxParams(UMaterialInstanceDynamic* M, int32 Kind, float P, float Seed, float Alpha, const FLinearColor& Col, const FLinearColor& Hot, float Swap, float Dir)
{
	if (!M) { return; }
	M->SetScalarParameterValue(TEXT("Shape"), float(Kind)); M->SetScalarParameterValue(TEXT("P"), P); M->SetScalarParameterValue(TEXT("Seed"), Seed); M->SetScalarParameterValue(TEXT("Alpha"), Alpha);
	M->SetScalarParameterValue(TEXT("Swap"), Swap); M->SetScalarParameterValue(TEXT("Dir"), Dir);
	M->SetVectorParameterValue(TEXT("Col"), Col); M->SetVectorParameterValue(TEXT("Hot"), Hot);
}

void FGkGame::TickFx(int32 Steps)
{
	const float RotSign = Tn(TEXT("rotsign"), -1.f), ArcOff = Tn(TEXT("arcoff"), 90.f), Swap = Tn(TEXT("uvswap"), 0.f), Scale = Tn(TEXT("fxscale"), 0.46f);
	for (FGkFx& F : Fx)
	{
		if (!F.bOn) { continue; }
		F.Age += float(Steps); const float K = F.Age / F.Life;
		if (K >= 1.f) { F.bOn = false; if (F.Quad) { F.Quad->SetVisibility(false); } continue; }
		F.Pos += F.Vel * double(Steps);
		if (!F.Quad) { continue; }
		const float Grow = 1.f - (1.f - K) * (1.f - K) * (1.f - K), Size = FMath::Lerp(F.Size0, F.Size1, Grow) * (F.Kind == 8 ? 1.f : Scale);
		const float Fade = F.Kind == 0 ? (1.f - K) : (1.f - GkEase((K - 0.7f) / 0.3f));
		GkSetQuad(F.Quad, F.Pos, F.Rot * RotSign + ((F.Kind == 4 || F.Kind == 5) ? ArcOff : 0.f), Size, Size * F.Stretch);
		GkFxParams(F.Mat, F.Kind, K, F.Seed, F.Alpha * Fade, F.Col, F.Core, Swap, 1.f);
	}
	// Corpse Candle: ghost fire with a real light that travels with it
	for (int32 i = 0; i < 2; i++)
	{
		const gk::Shot& Sh = S.shot[i]; const bool bOn = Sh.on != 0;
		for (int32 k = 0; k < 2; k++) { if (ShotQuad[i][k] && !bOn) { ShotQuad[i][k]->SetVisibility(false); } }
		if (ShotLight[i]) { ShotLight[i]->SetIntensity(bOn ? 260.f + 60.f * FMath::Sin(Tick60 * 0.9f) : 0.f); }
		if (!bOn) { continue; }
		const FVector Where = At(Sh.x, Sh.y, 22.0); const float Face = Sh.vx > 0 ? 0.f : 180.f;
		if (ShotQuad[i][0]) { GkSetQuad(ShotQuad[i][0], Where + FVector(-6.0, 0.0, 0.0), 0.f, 330.f, 330.f); GkFxParams(ShotMat[i][0], 0, 0.f, 0.f, 0.5f, Hue(i), HueCore(i), Swap, 1.f); }
		if (ShotQuad[i][1]) { GkSetQuad(ShotQuad[i][1], Where, Face * RotSign + ArcOff, 120.f, 200.f); GkFxParams(ShotMat[i][1], 5, 0.f, float(i) * 0.37f, 1.f, Hue(i), HueCore(i), Swap, 1.f); }
		if (ShotLight[i]) { ShotLight[i]->SetWorldLocation(Where + FVector(50.0, 0.0, 0.0)); ShotLight[i]->SetLightColor(Hue(i)); }
	}
}

// ------------------------------------------------------------------------------------------------ the fighters
/** Which drawing a fighter shows this frame: a clip and a time within it, plus any whole-body tumble or turn. */
void FGkGame::Pick(int32 Index, FName& Clip, int32& Time, float& Roll, float& Spin) const
{
	const gk::Player& P = S.p[Index]; Clip = TEXT("Idle"); Time = 0; Roll = 0.f; Spin = 0.f;
	switch (P.st)
	{
		case gk::IDLE:
			if (P.crouch) { Clip = TEXT("Crouch"); }
			else if (P.vx != 0) { Clip = (P.vx * P.face > 0) ? TEXT("Walk") : TEXT("WalkBack"); Time = int32(S.frame); }
			else { Time = int32(S.frame) + Index * 18; }
			break;
		case gk::PREJUMP: Clip = TEXT("PreJump"); break;
		case gk::LAND: Clip = TEXT("Land"); Time = P.sf; break;
		case gk::AIR: case gk::TECH:
			Clip = TEXT("Air"); Time = FMath::Clamp(int32((900.f - P.vy) / 1500.f * 6.f), 0, 5);
			if (P.st == gk::TECH) { Roll = -360.f * GkEase(P.sf / 14.f); }
			break;
		case gk::HELPLESS: Clip = TEXT("Launch"); Roll = -40.f; break;
		case gk::DASH: Clip = TEXT("Dash"); Time = P.sf; break;
		case gk::BACKDASH: Clip = TEXT("BackDash"); Time = P.sf; break;
		case gk::BLOCK: Clip = P.crouch ? TEXT("BlockC") : TEXT("BlockS"); break;
		case gk::PARRY: Clip = TEXT("Parry"); Time = P.sf; break;
		case gk::PARRIED: Clip = TEXT("Hit"); break;
		case gk::HIT: Clip = TEXT("Hit"); Time = FMath::Clamp(14 - int32(P.stun), 0, 14); break;
		case gk::LAUNCH: case gk::KO_:
			Clip = TEXT("Launch"); Roll = -FMath::Clamp((1200.f - P.vy) / 24.f, 0.f, 110.f);
			if (P.y <= 0 && P.st == gk::KO_) { Clip = TEXT("Down"); Roll = 0.f; }
			break;
		case gk::DOWN_: Clip = TEXT("Down"); break;
		case gk::RISE_: Clip = TEXT("Rise"); Time = P.sf; break;
		case gk::GRAB: Clip = TEXT("Grab"); Time = P.sf; break;
		case gk::HELD: Clip = TEXT("Held"); break;
		case gk::WIN_: Clip = TEXT("Win"); break;
		case gk::ATK:
		{
			const gk::Move& M = gk::move(P.mv); const FGkMoveAnim& A = GkLib().Move(FName(GkMvKey(P.mv))); const int32 Su = FMath::Max<int32>(1, M.su), Fr = P.mf;
			Clip = GkMvKey(P.mv); Time = Fr;
			if (P.mv == gk::RISE && Fr > Su) { Spin = 360.f * GkEase(float(Fr - Su) / 16.f); }
			if (P.mv == gk::WHIRL) { Spin = Fr * 34.f; }
			if (A.Spin != 0.f && Fr > Su) { Spin = 360.f * A.Spin * GkEase(float(Fr - Su) / FMath::Max<int32>(1, M.ac + M.rc / 2)); }
			break;
		}
		default: break;
	}
}

/** Puts one baked frame onto a mesh. */
void FGkGame::Pose(UPoseableMeshComponent* Mesh, int32 FrameIndex, bool bMirror, TArray<FTransform>& Bones) const
{
	const FGkAnim& An = GkAnim(); Bones = Rig.RefCS;
	for (int32 a = 0; a < An.Bones.Num(); a++)
	{
		const int32* Ri = Rig.Index.Find(An.Bones[a]); if (!Ri) { continue; }
		Bones[*Ri] = Rig.RefCS[*Ri] * An.Delta(FrameIndex, bMirror, a);
	}
	if (Mesh && Rig.IsValid()) { for (int32 b = 0; b < Bones.Num(); b++) { Mesh->SetBoneTransformByName(Rig.Names[b], Bones[b], EBoneSpaces::ComponentSpace); } }
}

void FGkGame::ShowFighter(int32 Index, int32 Steps)
{
	FGkView& Vw = V[Index]; const gk::Player& P = S.p[Index]; if (!Vw.Mesh) { return; }
	// every frame is a drawing: the clip and time pick one, and it is shown as it is, with no blending between
	FName Clip; int32 Time = 0; float Roll = 0.f, Spin = 0.f; Pick(Index, Clip, Time, Roll, Spin);
	if (!Vw.Frozen.IsNone()) { Clip = Vw.Frozen; Time = Vw.FrozenTime; Roll = 0.f; Spin = 0.f; }
	int32 Drawing = GkAnim().Frame(Clip, Time); if (Drawing < 0) { Drawing = GkAnim().Frame(TEXT("Idle"), 0); }
	if (P.hitstop > 0 && Vw.Drawing >= 0 && Vw.Frozen.IsNone()) { Drawing = Vw.Drawing; }
	Vw.Drawing = Drawing;
	// a fighter facing right is shown in mirror image, so both always show the camera their chest
	const bool bRight = P.face > 0; if (bRight) { Spin = -Spin; }
	Pose(Vw.Mesh, Drawing, bRight, Vw.Bones);
	const bool bBones = Rig.IsValid();
	const double Sc = 1.082, Hz = 92.0;
	FVector Jit = FVector::ZeroVector;
	if (P.hitstop > 0 && (P.st == gk::HIT || P.st == gk::LAUNCH || P.st == gk::BLOCK)) { Jit.Y = ((Tick60 & 1) ? 1.0 : -1.0) * FMath::Min(5.0, P.hitstop * 0.7); }
	// the core's line of play is the world Y axis. Facing right in the core is toward world -Y.
	const FVector Base = FVector(Index ? -6.0 : 6.0, -double(P.x) / gk::F, double(P.y) / gk::F) + Jit;
	const FQuat Yaw = FRotator(0.0, bRight ? 180.0 : 0.0, 0.0).Quaternion();
	// tumble about the hips rather than the feet
	const FQuat Tumble = FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(Roll)) * FQuat(FVector(0, 0, 1), FMath::DegreesToRadians(Spin));
	const FVector Pivot = FVector(0, 0, Hz) - Tumble.RotateVector(FVector(0, 0, Hz));
	const FVector Loc = Base + Yaw.RotateVector(Pivot); const FQuat Rot = Yaw * Tumble;
	Vw.Mesh->SetWorldLocationAndRotation(Loc, Rot); Vw.Mesh->SetWorldScale3D(FVector(Sc));
	const bool bGone = P.st == gk::ATK && P.mv == gk::VANISH && P.mf > 5 && P.mf < 12;
	Vw.Mesh->SetVisibility(!bGone && !bSheet);
	if (Vw.BodyMat) { Vw.BodyMat->SetScalarParameterValue(TEXT("Flash"), Vw.Flash > 0 ? 1.f : 0.f); }

	// afterimages: he leaves ghosts of himself when he moves faster than the eye
	const bool bTrail = P.st == gk::DASH || P.st == gk::BACKDASH || P.st == gk::TECH
		|| (P.st == gk::ATK && (P.mv == gk::HOME || P.mv == gk::VANISH || P.mv == gk::DIVE || P.mv == gk::SURGE || P.mv == gk::RISE || P.mv >= gk::SUPER1));
	Vw.GhostClock += Steps;
	if (bTrail && Vw.GhostClock >= 3 && !bGone && !bSheet)
	{
		Vw.GhostClock = 0;
		const int32 g = Vw.GhostNext; Vw.GhostNext = (g + 1) % 4;
		if (UPoseableMeshComponent* Gh = Vw.Ghost[g])
		{
			if (bBones) { for (int32 b = 0; b < Vw.Bones.Num(); b++) { Gh->SetBoneTransformByName(Rig.Names[b], Vw.Bones[b], EBoneSpaces::ComponentSpace); } }
			Gh->SetWorldLocationAndRotation(Loc + FVector(-45.0, 0.0, 0.0), Rot); Gh->SetWorldScale3D(FVector(Sc)); Gh->SetVisibility(true); Vw.GhostAge[g] = 0.f;
		}
	}
	for (int32 g = 0; g < 4; g++)
	{
		if (Vw.GhostAge[g] > 50.f || !Vw.Ghost[g]) { continue; }
		Vw.GhostAge[g] += float(Steps); const float K = Vw.GhostAge[g] / 16.f;
		if (K >= 1.f) { Vw.Ghost[g]->SetVisibility(false); Vw.GhostAge[g] = 99.f; }
		else if (Vw.GhostMat[g]) { Vw.GhostMat[g]->SetScalarParameterValue(TEXT("Alpha"), FMath::Min(1.f, K * 5.f) * (1.f - K) * 0.6f); Vw.GhostMat[g]->SetVectorParameterValue(TEXT("Col"), Hue(Index)); }
	}
	// the health bar's trailing edge catches up after a moment
	const float Hp = FMath::Clamp(P.hp / float(gk::MAX_HP), 0.f, 1.f);
	Vw.HpLag = (Hp > Vw.HpLag || ComboTimer[1 - Index] > 30) ? FMath::Max(Vw.HpLag, Hp) : FMath::Max(Hp, Vw.HpLag - 0.006f * Steps);
	if (Hp > Vw.HpLag) { Vw.HpLag = Hp; }
}

// ------------------------------------------------------------------------------------------------ the world and the lens
void FGkGame::ShowWorld(float DeltaSeconds)
{
	const float Rate = FMath::Clamp(DeltaSeconds * 60.f, 0.f, 4.f);
	Shake *= FMath::Pow(0.86f, Rate); Punch *= FMath::Pow(0.9f, Rate); Flash *= FMath::Pow(0.84f, Rate); White *= FMath::Pow(0.86f, Rate); HitGlow *= FMath::Pow(0.78f, Rate);

	// the weather follows the fight: a red dusk while both stand strong, a storm as one of them nears the end
	const float HpMin = FMath::Min(S.p[0].hp, S.p[1].hp) / float(gk::MAX_HP);
	float StormWant = FMath::Clamp((0.6f - HpMin) / 0.35f, 0.f, 1.f);
	if (S.round >= 3) { StormWant = FMath::Max(StormWant, 0.65f); }
	if (S.phase == gk::PH_INTRO && S.round < 3) { StormWant = 0.f; }
	StormWant = Tn(TEXT("storm"), StormWant);
	Storm = FMath::Lerp(Storm, StormWant, FMath::Clamp(DeltaSeconds * Tn(TEXT("stormrate"), 0.8f), 0.f, 1.f));
	Dim = FMath::Lerp(Dim, (S.freeze > 0 || SuperTimer > 70) ? 1.f : 0.f, FMath::Clamp(DeltaSeconds * 14.f, 0.f, 1.f));
	const float Lit = 1.f - 0.92f * Dim, Bolt = FMath::Min(1.f, Flash);
	if (Sun) { Sun->SetIntensity((FMath::Lerp(3.2f, 0.7f, Storm) + Bolt * 14.f) * Lit); Sun->SetLightColor(GkMix(FLinearColor(1.f, 0.36f, 0.2f), FLinearColor(0.6f, 0.68f, 1.f), FMath::Max(Storm, Bolt))); }
	if (Fill) { Fill->SetIntensity(FMath::Lerp(0.45f, 1.5f, Storm) * Lit); }
	if (Sky) { Sky->SetIntensity(FMath::Lerp(0.7f, 0.5f, Storm) * Lit); }
	if (Fog) { Fog->SetFogDensity(FMath::Lerp(0.012f, 0.03f, Storm)); Fog->SetFogInscatteringColor(GkMix(FLinearColor(0.22f, 0.03f, 0.05f), FLinearColor(0.05f, 0.07f, 0.15f), Storm) * Lit); }
	if (SkyMat) { SkyMat->SetScalarParameterValue(TEXT("Storm"), Storm); SkyMat->SetScalarParameterValue(TEXT("Flash"), Bolt); SkyMat->SetScalarParameterValue(TEXT("Dim"), Dim); }
	if (SunMat) { SunMat->SetVectorParameterValue(TEXT("Col"), GkMix(FLinearColor(3.2f, 0.16f, 0.07f), FLinearColor(0.5f, 0.6f, 0.85f), Storm)); SunMat->SetScalarParameterValue(TEXT("Glow"), Lit); }
	if (HitLight) { HitLight->SetWorldLocation(HitAt); HitLight->SetLightColor(HitCol); HitLight->SetIntensity(HitGlow * Tn(TEXT("hitlight"), 700.f)); }

	// the camera frames both fighters, pulls in when they close, and rises with them when the fight leaves the ground
	const double Y0 = -double(S.p[0].x) / gk::F, Y1 = -double(S.p[1].x) / gk::F, ZMax = FMath::Max(S.p[0].y, S.p[1].y) / double(gk::F);
	const double Sep = FMath::Abs(Y0 - Y1);
	// the distances were tuned for a 42 degree lens (tan 21 = 0.3839); a wider lens stands closer and shows more of the world behind
	const double Fov = Tn(TEXT("fov"), 58.f), Wide = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5)), Zoom = Wide / 0.3839;
	double Dist = FMath::Clamp(FMath::Max(540.0 + Sep * 1.02, (ZMax + 300.0) / 0.40), 900.0, 1750.0) / Zoom;
	Dist *= 1.0 - 0.07 * FMath::Min(1.f, Punch);
	const double HalfW = Wide * Dist, Edge = double(gk::STAGE) / gk::F + 90.0 - HalfW;
	const double MidY = Edge > 0.0 ? FMath::Clamp((Y0 + Y1) * 0.5, -Edge, Edge) : 0.0;
	FVector Want(Dist, MidY, FMath::Max(double(Tn(TEXT("camz"), 102.f)), ZMax * 0.5 + 76.0));
	if (Tune.Contains(TEXT("camx"))) { Want = FVector(Tn(TEXT("camx"), 1000.f), Tn(TEXT("camy"), 0.f), Tn(TEXT("camzfix"), 110.f)); CamPos = Want; }
	// a super cuts in close on whoever called it: low, ahead of him, the frame tilted
	double CamYaw = 180.0, CamRoll = 0.0; float CamFov = float(Fov); const bool bCut = S.freeze > 0 && SuperWho >= 0 && !Tune.Contains(TEXT("camx"));
	if (bCut)
	{
		const gk::Player& Q = S.p[SuperWho & 1]; const double Qy = -double(Q.x) / gk::F, Ahead = Q.face > 0 ? -1.0 : 1.0;
		Want = FVector(Tn(TEXT("cutx"), 330.f), Qy + Ahead * 45.0, double(Q.y) / gk::F + 118.0); CamPos = Want;
		CamRoll = Ahead * Tn(TEXT("cutroll"), 7.f); CamFov = 40.f;
	}
	CamPos = FMath::Lerp(CamPos, Want, FMath::Clamp(DeltaSeconds * 7.0, 0.0, 1.0));
	if (Cam)
	{
		const FVector Sh(0.0, FMath::Sin(Tick60 * 1.7) * Shake, FMath::Cos(Tick60 * 2.3) * Shake * 0.7);
		Cam->SetActorLocation(CamPos + Sh); Cam->SetActorRotation(FRotator(bCut ? 5.0 : double(Tn(TEXT("campitch"), 2.f)), CamYaw, CamRoll));
		UCameraComponent* CC = Cam->GetCameraComponent(); CC->SetFieldOfView(CamFov);
		CC->PostProcessBlendWeight = 1.f; FPostProcessSettings& PP = CC->PostProcessSettings;
		// impact frames: the picture drops to hard black and white for an instant
		PP.bOverride_ColorSaturation = true; const double St = Storm; PP.ColorSaturation = Mono > 0 ? FVector4(0.0, 0.0, 0.0, 1.0) : FVector4(1.12 - 0.3 * St, 1.0 - 0.08 * St, 1.08 + 0.04 * St, 1.0);
		PP.bOverride_ColorContrast = true; PP.ColorContrast = Mono > 0 ? FVector4(1.8, 1.8, 1.8, 1.0) : FVector4(1.1, 1.08, 1.1, 1.0);
		PP.bOverride_SceneFringeIntensity = true; PP.SceneFringeIntensity = FMath::Min(4.f, 0.06f + Shake * 0.3f);
		PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.5f + 0.6f * Dim;
		PP.bOverride_DepthOfFieldFocalDistance = true; PP.DepthOfFieldFocalDistance = 0.f;
	}

	// weather drawn across the lens: embers on the calm evening, rain when the storm comes in
	const float Swap = Tn(TEXT("uvswap"), 0.f), Fall = Tn(TEXT("raindir"), 1.f);
	const double Depth[3] = { 90.0, -240.0, -110.0 }; const int32 Kinds[3] = { 6, 6, 7 };
	const float Alphas[3] = { Storm * 0.5f, Storm * 0.34f, (1.f - Storm) * 0.9f };
	for (int32 k = 0; k < 3; k++)
	{
		if (!SheetQuad[k]) { continue; }
		if (Alphas[k] < 0.02f) { SheetQuad[k]->SetVisibility(false); continue; }
		const double Far = CamPos.X - Depth[k];
		GkSetQuad(SheetQuad[k], FVector(Depth[k], CamPos.Y, CamPos.Z + Far * 0.035), 0.f, float(Far * Wide * 1.125 * 1.35), float(Far * Wide * 2.0 * 1.35));
		GkFxParams(SheetMat[k], Kinds[k], 0.f, 0.21f + k * 0.37f, Alphas[k] * Lit, k == 2 ? FLinearColor(6.f, 1.4f, 0.3f) : FLinearColor(0.9f, 1.1f, 1.5f), FLinearColor(9.f, 5.f, 1.5f), Swap, Fall);
	}
}

// ------------------------------------------------------------------------------------------------ test and tuning commands
/**
 * pause 0|1 · step N · in PLAYER BITS FRAMES · cpu PLAYER 0|1 · pose PLAYER Name|- · sheet Name Name ... · reload ·
 * lumen 0|1 · set key value · unset key · reset · hp PLAYER n · meter PLAYER n · place PLAYER x y · hud 0|1 · fx kind y z size rot life · state
 */
FString FGkGame::Debug(const FString& Command)
{
	TArray<FString> T; Command.ParseIntoArrayWS(T); if (T.Num() == 0) { return TEXT("?"); }
	auto Int = [&T](int32 i, int32 D) { return T.IsValidIndex(i) ? FCString::Atoi(*T[i]) : D; };
	auto Flt = [&T](int32 i, float D) { return T.IsValidIndex(i) ? FCString::Atof(*T[i]) : D; };
	const FString& C = T[0]; const int32 Pl = Int(1, 0) & 1;
	if (C == TEXT("pause")) { bPaused = Int(1, 1) != 0; return TEXT("ok"); }
	if (C == TEXT("step")) { bPaused = true; StepsOwed += Int(1, 1); return TEXT("ok"); }
	if (C == TEXT("in")) { ScriptIn[Pl] = Int(2, 0); ScriptFrames[Pl] = Int(3, 1); return TEXT("ok"); }
	if (C == TEXT("cpu")) { bCpu[Pl] = Int(2, 1) != 0; return TEXT("ok"); }
	if (C == TEXT("pose")) { V[Pl].Frozen = (T.Num() > 2 && T[2] != TEXT("-")) ? FName(*T[2]) : FName(); V[Pl].FrozenTime = Int(3, 0); return TEXT("ok"); }
	if (C == TEXT("reload")) { FString Err; const bool bOk = GkLib().Load(FPaths::ProjectContentDir() / TEXT("Gk/kagu.txt"), Err) && GkAnim().Load(FPaths::ProjectContentDir() / TEXT("Gk"), Err); return bOk ? FString::Printf(TEXT("ok %d frames %d clips"), GkAnim().NumFrames, GkAnim().Clips.Num()) : Err; }
	if (C == TEXT("set")) { if (T.Num() > 2) { Tune.Add(FName(*T[1]), Flt(2, 0.f)); } return TEXT("ok"); }
	if (C == TEXT("unset")) { if (T.Num() > 1) { Tune.Remove(FName(*T[1])); } return TEXT("ok"); }
	if (C == TEXT("reset")) { gk::init(S); OverTimer = 0; return TEXT("ok"); }
	if (C == TEXT("hp")) { S.p[Pl].hp = int16(FMath::Clamp(Int(2, gk::MAX_HP), 1, gk::MAX_HP)); return TEXT("ok"); }
	if (C == TEXT("meter")) { S.p[Pl].meter = int16(FMath::Clamp(Int(2, gk::MAX_METER), 0, gk::MAX_METER)); return TEXT("ok"); }
	if (C == TEXT("place")) { S.p[Pl].x = Int(2, 0) * gk::F; S.p[Pl].y = Int(3, 0) * gk::F; return TEXT("ok"); }
	if (C == TEXT("hud")) { bHud = Int(1, 1) != 0; return TEXT("ok"); }
	if (C == TEXT("mode")) { Mode = FMath::Clamp(Int(1, 1), 0, 2); return TEXT("ok"); }
	if (C == TEXT("menu")) { bMenu = Int(1, 1) != 0; MenuPage = FMath::Clamp(Int(2, 0), 0, 1); return TEXT("ok"); }
	if (C == TEXT("dummy")) { Dummy = FMath::Clamp(Int(1, 0), 0, 4); return TEXT("ok"); }
	if (C == TEXT("lumen")) { SetLumen(Int(1, 1) != 0); return TEXT("ok"); }
	if (C == TEXT("fx"))
	{
		FGkFx& F = Spawn(Int(1, 1), FVector(30.0, Flt(2, 0.f), Flt(3, 120.f)), Flt(4, 200.f), Flt(6, 90.f), Hue(0), HueCore(0));
		F.Rot = Flt(5, 0.f); F.Size1 = F.Size0; F.Stretch = Flt(7, 1.f); return TEXT("ok");
	}
	if (C == TEXT("sheet"))
	{
		const int32 Want = FMath::Min(T.Num() - 1, 12); bSheet = Want > 0;
		while (Sheet.Num() < Want)
		{
			UPoseableMeshComponent* M = GkMakeSkel(Owner, SkMesh);
			UMaterialInterface* Body = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mat/MI_Kagu.MI_Kagu"));
			if (Body) { for (int32 m = 0; m < FMath::Max(1, M->GetNumMaterials()); m++) { M->SetMaterial(m, Body); } }
			if (InkMat) { M->SetOverlayMaterial(InkMat); }
			Sheet.Add(M);
		}
		const float Gap = Tn(TEXT("sheetgap"), 170.f), Turn = Tn(TEXT("sheetyaw"), 0.f);
		for (int32 k = 0; k < Sheet.Num(); k++)
		{
			const bool bShow = k < Want; Sheet[k]->SetVisibility(bShow); if (!bShow) { continue; }
			// "Clip" or "Clip:time"
			const FString Full = T[k + 1]; FString Nm = Full, Tm; int32 Tk = 0; if (Full.Split(TEXT(":"), &Nm, &Tm)) { Tk = FCString::Atoi(*Tm); }
			TArray<FTransform> Bn; Pose(Sheet[k], FMath::Max(0, GkAnim().Frame(FName(*Nm), Tk)), false, Bn);
			Sheet[k]->SetWorldLocationAndRotation(FVector(0.0, (k - (Want - 1) * 0.5) * -Gap, 0.0), FRotator(0.0, Turn, 0.0)); Sheet[k]->SetWorldScale3D(FVector(1.082));
		}
		return FString::Printf(TEXT("ok %d"), Want);
	}
	if (C == TEXT("state"))
	{
		FString Out = FString::Printf(TEXT("frame %u phase %d round %d time %d freeze %d storm %.2f ms %.1f step %.2f show %.2f"), S.frame, int32(S.phase), int32(S.round), int32(S.time), int32(S.freeze), Storm, FrameMs, StepMs, ShowMs);
		for (int32 i = 0; i < 2; i++)
		{
			const gk::Player& P = S.p[i];
			Out += FString::Printf(TEXT(" | p%d %s mv %s mf %d x %d y %d hp %d meter %d face %d combo %d"), i, ANSI_TO_TCHAR(gk::stateName(P.st)), GkMvKey(P.mv), int32(P.mf), P.x / gk::F, P.y / gk::F, int32(P.hp), int32(P.meter), int32(P.face), int32(P.combo));
		}
		return Out;
	}
	return TEXT("unknown command");
}
