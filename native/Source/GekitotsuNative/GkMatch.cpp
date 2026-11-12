#include "GkMatch.h"
#include "GkPad.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

UAnimSequence* AGkMatch::Anim(const TCHAR* Name) const { const TObjectPtr<UAnimSequence>* A = Anims.Find(FName(Name)); return A ? A->Get() : nullptr; }

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

	// a neutral place to fight: a floor, a marker at each end of the stage, daylight
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto Piece = [this](UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this); C->SetMobility(EComponentMobility::Movable); C->SetupAttachment(RootComponent);
		if (Mesh) { C->SetStaticMesh(Mesh); } C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent(); C->SetWorldLocation(Loc); C->SetWorldScale3D(Scale);
		return C;
	};
	const double Half = double(gk::STAGE) * Cm;
	Piece(Plane, FVector(200.0, 0.0, 0.0), FVector(30.0, 40.0, 1.0));
	Piece(Cube, FVector(60.0, Half + 25.0, 150.0), FVector(0.5, 0.5, 3.0)); Piece(Cube, FVector(60.0, -Half - 25.0, 150.0), FVector(0.5, 0.5, 3.0));
	for (int32 k = 0; k < 4; k++) { ShotMesh[k] = Piece(Ball, FVector::ZeroVector, FVector(0.45)); ShotMesh[k]->SetCastShadow(false); ShotMesh[k]->SetVisibility(false); }
	if (W)
	{
		if (ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector(0.0, 0.0, 800.0), FRotator(-46.0, 28.0, 0.0)))
		{
			if (ULightComponent* L = Sun->GetLightComponent()) { L->SetMobility(EComponentMobility::Movable); L->SetIntensity(6.f); }
			if (UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(Sun->GetLightComponent())) { D->SetAtmosphereSunLight(true); }
		}
		USkyAtmosphereComponent* Air = NewObject<USkyAtmosphereComponent>(this); Air->SetupAttachment(RootComponent); Air->RegisterComponent();
		if (ASkyLight* Sky = W->SpawnActor<ASkyLight>(FVector(0.0, 0.0, 600.0), FRotator::ZeroRotator))
		{
			if (USkyLightComponent* L = Sky->GetLightComponent()) { L->SetMobility(EComponentMobility::Movable); L->SetRealTimeCapture(true); L->SetIntensity(1.f); }
		}
	}

	// the fighters: Unreal's own mannequins stand in until the real characters are made
	static const TCHAR* Names[] = { TEXT("MM_Idle"), TEXT("Walk/MF_Unarmed_Walk_Fwd"), TEXT("Walk/MF_Unarmed_Walk_Bwd"), TEXT("Jog/MF_Unarmed_Jog_Fwd"), TEXT("Jog/MF_Unarmed_Jog_Bwd"), TEXT("Jump/MM_Jump"), TEXT("Jump/MM_Fall_Loop"),
		TEXT("Jump/MM_Land"), TEXT("Jump/MM_Dash"), TEXT("Attack/MM_Attack_01"), TEXT("Attack/MM_Attack_02"), TEXT("Attack/MM_Attack_03"), TEXT("Attack/MM_ChargedAttack") };
	for (const TCHAR* N : Names)
	{
		const FString Leaf = FPaths::GetCleanFilename(N); const FString Path = FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/%s.%s"), N, *Leaf);
		if (UAnimSequence* A = LoadObject<UAnimSequence>(nullptr, *Path)) { Anims.Add(FName(*Leaf), A); }
	}
	if (UAnimSequence* A = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01.MM_Death_Back_01"))) { Anims.Add(TEXT("MM_Death_Back_01"), A); }
	static const TCHAR* Meshes[2] = { TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple") };
	for (int32 i = 0; i < 2; i++)
	{
		USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(this); C->SetupAttachment(RootComponent);
		C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		if (USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, Meshes[i])) { C->SetSkeletalMeshAsset(M); }
		C->SetAnimationMode(EAnimationMode::AnimationSingleNode); C->SetBoundsScale(6.f);
		Body[i] = C;
	}
	if (W)
	{
		Cam = W->SpawnActor<ACameraActor>(CamPos, FRotator::ZeroRotator);
		if (Cam)
		{
			Cam->GetCameraComponent()->SetFieldOfView(30.f); Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
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
	switch (E.type)
	{
		case gk::E_HIT:
			Sparks.Add(FSpark{ At(E.x, E.y), 0, 0 }); Shake = FMath::Max(Shake, 2.f + E.a * 2.f + ((E.f & gk::H_KO) ? 10.f : 0.f));
			Combo[Who] = E.c; ComboDmg[Who] = E.d; ComboTimer[Who] = 80;
			if (E.f & gk::H_IMPACT) { Remark(Who, (E.f & gk::H_CRUMPLE) ? TEXT("CRUMPLE!") : TEXT("BREACH!")); }
			else if (E.f & gk::H_FREE) { Remark(Who, TEXT("CHASE")); }
			else if (E.f & gk::H_LOW) { Remark(Who, TEXT("LOW!")); }
			else if (E.f & gk::H_OVERHEAD) { Remark(Who, TEXT("OVERHEAD!")); }
			else if (E.e == 2 && E.c <= 1) { Remark(Who, TEXT("PUNISH")); }
			else if (E.e == 1 && E.c <= 1) { Remark(Who, TEXT("COUNTER")); }
			break;
		case gk::E_BLOCK: Sparks.Add(FSpark{ At(E.x, E.y), 0, 1 }); Shake = FMath::Max(Shake, 1.5f); break;
		case gk::E_PARRY: Sparks.Add(FSpark{ At(E.x, E.y), 0, 2 }); break;
		case gk::E_PPARRY: Sparks.Add(FSpark{ At(E.x, E.y), 0, 2 }); Remark(Who, TEXT("PERFECT PARRY")); break;
		case gk::E_ARMOR: Sparks.Add(FSpark{ At(E.x, E.y), 0, 1 }); Remark(Who, TEXT("ARMOR")); break;
		case gk::E_DICLASH: Say(TEXT("CLASH!"), 50); Shake = FMath::Max(Shake, 12.f); break;
		case gk::E_SPLAT: Remark(Who, E.a ? TEXT("STUN!") : TEXT("WALL SPLAT")); Shake = FMath::Max(Shake, 12.f); break;
		case gk::E_GBOUNCE: case gk::E_WBOUNCE: Shake = FMath::Max(Shake, 10.f); break;
		case gk::E_KD: Shake = FMath::Max(Shake, 3.f); break;
		case gk::E_RUSH: Remark(Who, TEXT("GHOST STEP")); break;
		case gk::E_BURNOUT: Remark(Who, TEXT("BURNOUT")); break;
		case gk::E_SUPER: Say(*FString(UTF8_TO_TCHAR(gk::moveName(E.c))).ToUpper(), 70); Shake = FMath::Max(Shake, 6.f); break;
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
/** Sets one fighter's animation to the frame the rules are on. Nothing plays by itself: every pose is asked for by time. */
void AGkMatch::ShowFighter(int32 i)
{
	USkeletalMeshComponent* C = Body[i]; if (!C) { return; }
	const gk::Fighter& P = S.p[i]; const float T60 = 1.f / 60.f;
	UAnimSequence* A = Anim(TEXT("MM_Idle")); float T = (S.frame + i * 20) * T60; bool bLoop = true; float Roll = 0.f, Sink = 0.f;
	auto Use = [&](const TCHAR* Name, float Time, bool bLoops) { if (UAnimSequence* Q = Anim(Name)) { A = Q; T = Time; bLoop = bLoops; } };
	switch (P.st)
	{
		case gk::IDLE:
			if (P.sub == gk::CROUCH) { Use(TEXT("MM_Land"), 0.06f, false); Sink = 34.f; }
			else if (P.sub == gk::WALKF) { Use(TEXT("MF_Unarmed_Walk_Fwd"), P.subf * T60, true); }
			else if (P.sub == gk::WALKB) { Use(TEXT("MF_Unarmed_Walk_Bwd"), P.subf * T60, true); }
			break;
		case gk::PREJUMP: Use(TEXT("MM_Jump"), P.sf * T60, false); break;
		case gk::JUMP: case gk::TECH: if (P.vy > 200) { Use(TEXT("MM_Jump"), 0.1f + P.sf * T60, false); } else { Use(TEXT("MM_Fall_Loop"), P.sf * T60, true); } break;
		case gk::LAND: Use(TEXT("MM_Land"), P.sf * T60, false); break;
		case gk::DASHF: Use(TEXT("MM_Dash"), P.sf * T60, false); break;
		case gk::DASHB: Use(TEXT("MF_Unarmed_Jog_Bwd"), P.sf * T60, true); break;
		case gk::RUN: case gk::DRUSH: Use(TEXT("MF_Unarmed_Jog_Fwd"), P.sf * T60 * 1.4f, true); break;
		case gk::BLOCK: Use(TEXT("MF_Unarmed_Walk_Bwd"), 0.2f, false); if (P.cb) { Sink = 34.f; } break;
		case gk::PARRY: case gk::PREC: Use(TEXT("MM_Idle"), 0.f, false); break;
		case gk::HIT: Use(TEXT("MM_Death_Back_01"), FMath::Min(0.3f, P.sf * T60), false); if (P.cb) { Sink = 30.f; } break;
		case gk::AHIT: case gk::THROWN: Use(TEXT("MM_Fall_Loop"), P.sf * T60, true); Roll = -70.f; break;
		case gk::KDOWN: if (UAnimSequence* Q = Anim(TEXT("MM_Death_Back_01"))) { A = Q; T = Q->GetPlayLength() - 0.02f; bLoop = false; } break;
		case gk::GETUP_: if (UAnimSequence* Q = Anim(TEXT("MM_Death_Back_01"))) { A = Q; T = Q->GetPlayLength() * FMath::Clamp(1.f - P.sf / float(gk::GETUP), 0.f, 1.f) * 0.6f; bLoop = false; } break;
		case gk::THROWING: Use(TEXT("MM_Attack_01"), 0.25f, false); break;
		case gk::ATK:
		{
			const gk::Move& M = gk::move(P.mv);
			// a stand-in swing, stretched over the move's own startup, active and recovery frames so that it lands when the move does
			const TCHAR* Name = (M.sp || M.sup) ? TEXT("MM_ChargedAttack") : M.lvl >= 1 && M.hsp >= 13 ? TEXT("MM_Attack_03") : M.lvl >= 1 ? TEXT("MM_Attack_02") : TEXT("MM_Attack_01");
			if (UAnimSequence* Q = Anim(Name))
			{
				const float Len = Q->GetPlayLength(), HitAt = 0.38f; const int32 Su = FMath::Max(1, M.su - 1), La = FMath::Max(Su, M.lastAct), End = M.t < 900 ? M.t : La + 24;
				const float K = P.mf <= Su ? HitAt * P.mf / Su : P.mf <= La ? HitAt : HitAt + (1.f - HitAt) * FMath::Min(1.f, float(P.mf - La) / FMath::Max(1, End - La));
				A = Q; T = Len * K; bLoop = false;
			}
			if (M.crouch) { Sink = 30.f; }
			break;
		}
		default: break;
	}
	if (A)
	{
		if (Showing[i] != A) { C->SetAnimation(A); C->Stop(); Showing[i] = A; }
		const float Len = A->GetPlayLength(); const float At0 = bLoop ? FMath::Fmod(T, FMath::Max(0.01f, Len)) : FMath::Clamp(T, 0.f, FMath::Max(0.f, Len - 0.001f));
		C->SetPosition(At0, false);
	}
	FVector Jit = FVector::ZeroVector;
	if (P.hs > 0 && (P.st == gk::HIT || P.st == gk::AHIT || P.st == gk::BLOCK)) { Jit.Y = ((Ticks & 1) ? 1.0 : -1.0) * FMath::Min(4.0, P.hs * 0.6); }
	// the mannequin faces its own +Y, which is the rules' "right"; the camera looks along +X
	C->SetWorldLocationAndRotation(At(P.x, P.y, i ? 8.0 : -8.0) + Jit - FVector(0.0, 0.0, Sink), FRotator(0.0, P.face > 0 ? 0.0 : 180.0, P.face > 0 ? Roll : -Roll));
}

void AGkMatch::Present(float Real)
{
	for (int32 i = 0; i < 2; i++) { ShowFighter(i); }
	for (int32 k = 0; k < 4; k++)
	{
		if (!ShotMesh[k]) { continue; }
		const bool bOn = k < S.nshots; ShotMesh[k]->SetVisibility(bOn); if (bOn) { ShotMesh[k]->SetWorldLocation(At(S.shot[k].x, S.shot[k].y)); }
	}
	// the camera frames both fighters the way the browser does: wide enough for the two of them and for the higher one's head
	const double Ax = S.p[0].x / double(gk::F), Bx = S.p[1].x / double(gk::F), Hy = FMath::Max(S.p[0].y, S.p[1].y) / double(gk::F);
	const double Tw = FMath::Max3(236.0, FMath::Min(480.0, FMath::Abs(Ax - Bx) + 150.0), FMath::Min(600.0, (Hy + 108.0) * 2.09)), Ty = Tw * 9.0 / 32.0 - 17.0, Unit = Cm * gk::F;
	const double HalfFov = FMath::DegreesToRadians(15.0), Dist = (Tw * 0.5 * Unit) / FMath::Tan(HalfFov);
	const double Edge = FMath::Max(0.0, (gk::STAGE / double(gk::F) + 30.0) - Tw * 0.5), Tx = FMath::Clamp((Ax + Bx) * 0.5, -Edge, Edge);
	const FVector Want(-Dist, Tx * Unit, Ty * Unit);
	CamPos = FMath::Lerp(CamPos, Want, FMath::Clamp(double(Real) * 9.0, 0.0, 1.0));
	Shake *= FMath::Pow(0.85f, FMath::Clamp(Real * 60.f, 0.f, 4.f));
	if (Cam) { Cam->SetActorLocation(CamPos + FVector(0.0, FMath::Sin(Ticks * 1.7) * Shake, FMath::Cos(Ticks * 2.3) * Shake * 0.7)); }

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
