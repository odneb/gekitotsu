#include "GkMatch.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Animation/SkeletalMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "InputCoreTypes.h"

// ------------------------------------------------------------------------------------------------ fighter
AGkFighter::AGkFighter()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
}

void AGkFighter::Setup(int32 InIndex)
{
	Index = InIndex;
	if (USkeletalMesh* Sk = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Kagu/Kagu_Run.Kagu_Run")))
	{
		Mesh->SetSkinnedAssetAndUpdate(Sk);
		Rig.Init(Sk->GetRefSkeleton());
	}
	UMaterialInterface* Body = LoadObject<UMaterialInterface>(nullptr, Index ? TEXT("/Game/Mat/MI_Shiro.MI_Shiro") : TEXT("/Game/Mat/MI_Kagu.MI_Kagu"));
	if (Body) { for (int32 i = 0; i < FMath::Max(1, Mesh->GetNumMaterials()); i++) { Mesh->SetMaterial(i, Body); } }
	if (UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mat/MI_Ink.MI_Ink"))) { Mesh->SetOverlayMaterial(Ink); }
	Mesh->SetBoundsScale(8.f);
	Mesh->SetCastShadow(true);
}

static float Ease(float K) { K = FMath::Clamp(K, 0.f, 1.f); return K * K * (3.f - 2.f * K); }

/** Which poses a move passes through, and which limb is steered onto its hitbox while it is active. */
struct FGkMoveAnim { EGkPose Wind, Strike; int32 Limb; };   // limb: 0 none, 1 left hand, 2 right hand, 3 left foot, 4 right foot
static FGkMoveAnim AnimOf(uint8 Mv)
{
	switch (Mv)
	{
		case gk::S5L: return { EGkPose::Idle2, EGkPose::LS, 1 };
		case gk::S5M: return { EGkPose::MW, EGkPose::MS, 2 };
		case gk::S5H: return { EGkPose::HW, EGkPose::HS, 2 };
		case gk::S2L: return { EGkPose::Crouch, EGkPose::CL, 3 };
		case gk::S2M: return { EGkPose::Crouch, EGkPose::CM, 3 };
		case gk::S2H: return { EGkPose::CHW, EGkPose::CHS, 2 };
		case gk::JL: return { EGkPose::Jump, EGkPose::JL, 1 };
		case gk::JM: return { EGkPose::Jump, EGkPose::JM, 3 };
		case gk::JH: return { EGkPose::Jump, EGkPose::JH, 4 };
		case gk::J2H: return { EGkPose::Fall, EGkPose::JD, 3 };
		case gk::FIRE: return { EGkPose::FireW, EGkPose::FireS, 0 };
		case gk::SURGE: return { EGkPose::SurgeW, EGkPose::SurgeS, 4 };
		case gk::RISE: return { EGkPose::CHW, EGkPose::RiseS, 2 };
		case gk::VANISH: return { EGkPose::Vanish, EGkPose::Vanish, 0 };
		case gk::WHIRL: return { EGkPose::Jump, EGkPose::Whirl, 0 };
		case gk::DIVE: return { EGkPose::Jump, EGkPose::Dive, 3 };
		case gk::HOME: return { EGkPose::HomeW, EGkPose::HomeS, 1 };
		case gk::THROW: return { EGkPose::Idle2, EGkPose::Grab, 0 };
		default: return { EGkPose::MW, EGkPose::SuperS, 2 };
	}
}

FGkPose AGkFighter::Target(const gk::State& S, float& Snap) const
{
	const gk::Player& P = S.p[Index]; Snap = 0.34f;
	const FGkPose& Idle = GkPose(EGkPose::Idle);
	switch (P.st)
	{
		case gk::IDLE:
		{
			if (P.crouch) { return GkPose(EGkPose::Crouch); }
			if (P.vx != 0) { const float K = 0.5f + 0.5f * FMath::Sin(S.frame * 0.26f); return GkBlend(GkPose(EGkPose::WalkA), GkPose(EGkPose::WalkB), Ease(K)); }
			return GkBlend(Idle, GkPose(EGkPose::Idle2), 0.5f + 0.5f * FMath::Sin(S.frame * 0.05f + Index * 2.f));
		}
		case gk::PREJUMP: return GkPose(EGkPose::Crouch);
		case gk::LAND: return GkBlend(GkPose(EGkPose::Crouch), Idle, 0.4f);
		case gk::AIR: case gk::TECH: case gk::HELPLESS:
		{
			FGkPose R = GkBlend(GkPose(EGkPose::Jump), GkPose(EGkPose::Fall), Ease((900.f - P.vy) / 1500.f));
			if (P.st == gk::TECH) { R.Roll = -360.f * Ease(P.sf / 14.f); Snap = 1.f; }
			if (P.st == gk::HELPLESS) { R = GkPose(EGkPose::Launch); R.Roll = -40.f; }
			return R;
		}
		case gk::DASH: Snap = 0.6f; return GkPose(EGkPose::Dash);
		case gk::BACKDASH: Snap = 0.6f; return GkPose(EGkPose::BackDash);
		case gk::BLOCK: Snap = 0.8f; return GkPose(P.crouch ? EGkPose::BlockC : EGkPose::BlockS);
		case gk::PARRY: Snap = 0.8f; return P.sf <= 10 ? GkPose(EGkPose::Parry) : GkBlend(GkPose(EGkPose::Parry), Idle, Ease((P.sf - 10) / 18.f));
		case gk::HIT: Snap = 0.9f; return GkBlend(GkPose(EGkPose::Hit), Idle, Ease(1.f - P.stun / 14.f) * 0.8f);
		case gk::LAUNCH: case gk::KO_: { FGkPose R = GkPose(EGkPose::Launch); R.Roll = -FMath::Clamp((1200.f - P.vy) / 24.f, 0.f, 110.f); if (P.y <= 0 && P.st == gk::KO_) { R = GkPose(EGkPose::Down); } Snap = 0.7f; return R; }
		case gk::DOWN_: Snap = 0.8f; return GkPose(EGkPose::Down);
		case gk::RISE_: return GkBlend(GkPose(EGkPose::Rise), Idle, Ease((P.sf - 9) / 9.f));
		case gk::GRAB: Snap = 0.8f; return P.sf < 20 ? GkPose(EGkPose::Grab) : GkPose(EGkPose::MS);
		case gk::HELD: Snap = 0.8f; return GkPose(EGkPose::Held);
		case gk::WIN_: return GkPose(EGkPose::Win);
		case gk::ATK:
		{
			const gk::Move& M = gk::move(P.mv); const FGkMoveAnim A = AnimOf(P.mv);
			const FGkPose& Base = (P.y > 0) ? GkPose(EGkPose::Jump) : ((M.flags & gk::CROUCHING) ? GkPose(EGkPose::Crouch) : Idle);
			FGkPose R; const int32 Su = FMath::Max<int32>(1, M.su), F = P.mf;
			if (F <= Su)
			{
				// wind up for most of the startup, then snap into the strike over the last three frames
				const int32 SnapAt = FMath::Max(1, Su - 3);
				R = F <= SnapAt ? GkBlend(Base, GkPose(A.Wind), Ease(float(F) / SnapAt)) : GkBlend(GkPose(A.Wind), GkPose(A.Strike), Ease(float(F - SnapAt) / (Su - SnapAt)));
				Snap = 0.85f;
			}
			else if (F <= Su + M.ac || P.mv == gk::HOME || P.mv == gk::DIVE) { R = GkPose(A.Strike); Snap = 1.f; }
			else { R = GkBlend(GkPose(A.Strike), Base, Ease(float(F - Su - M.ac) / FMath::Max<int32>(1, M.rc))); Snap = 0.6f; }
			// steer the striking limb onto the hitbox so what you see is what hits
			if (A.Limb && F > Su - 2 && F <= Su + M.ac + 2 && M.hb.w > 0)
			{
				const FVector T(A.Limb == 1 || A.Limb == 3 ? 6.0 : -6.0, M.hb.x + M.hb.w * 0.82, FMath::Max(8.0, double(M.hb.y + M.hb.h * (M.lvy > 1500 ? 0.9 : 0.5))));
				if (A.Limb == 1) { R.HL = T; } else if (A.Limb == 2) { R.HR = T; } else if (A.Limb == 3) { R.FL = T; } else { R.FR = T; }
			}
			if (P.mv == gk::RISE && F > Su) { R.Spin = 360.f * Ease(float(F - Su) / 16.f); }
			if (P.mv == gk::WHIRL) { R.Spin = F * 34.f; }
			if (P.mv == gk::HOME && F > Su) { R.Roll = 0.f; }
			return R;
		}
		default: return Idle;
	}
}

void AGkFighter::Present(const gk::State& S, int32 Frame)
{
	const gk::Player& P = S.p[Index];
	float Snap = 0.34f; const FGkPose T = Target(S, Snap);
	// poses are held and changed in steps rather than tweened on every frame: it reads as drawn animation
	if (!bHasPose) { Cur = T; bHasPose = true; }
	else if (P.hitstop == 0) { Cur = GkBlend(Cur, T, Snap); }
	Rig.Solve(Cur, Bones);
	if (Rig.IsValid())
	{
		for (int32 b = 0; b < Bones.Num(); b++) { Mesh->SetBoneTransformByName(Rig.Names[b], Bones[b], EBoneSpaces::ComponentSpace); }
	}
	// the core's line of play is the world Y axis. Facing right in the core is toward world -Y.
	const double Sc = 1.082; const bool bRight = P.face > 0;
	FVector Jit = FVector::ZeroVector;
	if (P.hitstop > 0 && (P.st == gk::HIT || P.st == gk::LAUNCH || P.st == gk::BLOCK)) { Jit.Y = ((Frame & 1) ? 1.0 : -1.0) * FMath::Min(4.0, P.hitstop * 0.6); }
	SetActorLocation(FVector(Index ? -6.0 : 6.0, -double(P.x) / gk::F, double(P.y) / gk::F) + Jit);
	SetActorRotation(FRotator(0.0, bRight ? 180.0 : 0.0, 0.0));
	// mirrored for the fighter facing left, so both always show the camera their chest
	Mesh->SetRelativeScale3D(FVector(bRight ? Sc : -Sc, Sc, Sc));
	Mesh->SetRelativeLocation(FVector(0, 0, Cur.Roll != 0.f ? 85.0 : 0.0));
	Mesh->SetRelativeRotation(FRotator(0.0, Cur.Spin, 0.0) + FRotator(0.0, 0.0, 0.0));
	if (Cur.Roll != 0.f)
	{
		// tumble about the hips rather than the feet
		const FQuat Q = FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(Cur.Roll)) * FQuat(FVector(0, 0, 1), FMath::DegreesToRadians(Cur.Spin));
		Mesh->SetRelativeRotation(Q); Mesh->SetRelativeLocation(FVector(0, 0, 85.0) - Q.RotateVector(FVector(0, 0, 85.0)));
	}
	const bool bGone = P.st == gk::ATK && P.mv == gk::VANISH && P.mf > 5 && P.mf < 12;
	Mesh->SetVisibility(!bGone);
}

// ------------------------------------------------------------------------------------------------ match
AGkMatch::AGkMatch() { PrimaryActorTick.bCanEverTick = true; Fighters[0] = nullptr; Fighters[1] = nullptr; }

void AGkMatch::BeginPlay()
{
	Super::BeginPlay(); gk::init(State); Accumulator = 0.0;
	UWorld* W = GetWorld(); if (!W) { return; }
	// the figures placed in the level are for looking at in the editor; the match brings its own
	for (TActorIterator<ASkeletalMeshActor> It(W); It; ++It) { It->SetActorHiddenInGame(true); }
	for (int32 i = 0; i < 2; i++) { Fighters[i] = W->SpawnActor<AGkFighter>(FVector::ZeroVector, FRotator::ZeroRotator); if (Fighters[i]) { Fighters[i]->Setup(i); } }
	Camera = W->SpawnActor<ACameraActor>(CamPos, FRotator(0, 180, 0));
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(42.f); Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
		if (APlayerController* PC = W->GetFirstPlayerController()) { PC->SetViewTarget(Camera); }
	}
}

uint16 AGkMatch::ReadPlayer() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr; if (!PC) { return 0; }
	auto K = [PC](const FKey& Key) { return PC->IsInputKeyDown(Key); };
	uint16 In = 0;
	const float SX = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX), SY = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	// on screen, left is toward world +Y, which is the core's negative x
	if (K(EKeys::A) || K(EKeys::Left) || K(EKeys::Gamepad_DPad_Left) || SX < -0.5f) { In |= gk::LEFT; }
	if (K(EKeys::D) || K(EKeys::Right) || K(EKeys::Gamepad_DPad_Right) || SX > 0.5f) { In |= gk::RIGHT; }
	if (K(EKeys::W) || K(EKeys::Up) || K(EKeys::SpaceBar) || K(EKeys::Gamepad_DPad_Up) || SY > 0.5f) { In |= gk::UP; }
	if (K(EKeys::S) || K(EKeys::Down) || K(EKeys::Gamepad_DPad_Down) || SY < -0.5f) { In |= gk::DOWN; }
	// leverless layout. Top row: Light, Medium, Heavy, Special. Bottom row: Parry, Throw, Slipstream, Super.
	if (K(EKeys::J) || K(EKeys::Gamepad_FaceButton_Left)) { In |= gk::BL; }
	if (K(EKeys::K) || K(EKeys::Gamepad_FaceButton_Top)) { In |= gk::BM; }
	if (K(EKeys::L) || K(EKeys::Gamepad_RightShoulder)) { In |= gk::BH; }
	if (K(EKeys::I) || K(EKeys::Gamepad_LeftShoulder)) { In |= gk::BS; }
	if (K(EKeys::U) || K(EKeys::Gamepad_FaceButton_Bottom)) { In |= gk::BP; }
	if (K(EKeys::O) || K(EKeys::Gamepad_FaceButton_Right)) { In |= gk::BL | gk::BM; }
	if (K(EKeys::H) || K(EKeys::Gamepad_RightTrigger)) { In |= gk::BM | gk::BH; }
	if (K(EKeys::P) || K(EKeys::Gamepad_LeftTrigger)) { In |= gk::BH | gk::BS; }
	return In;
}

/** A sparring partner. It only reads the same state a player sees and presses the same buttons. */
uint16 AGkMatch::Cpu(int32 Me)
{
	const gk::Player& P = State.p[Me]; const gk::Player& O = State.p[1 - Me];
	Rng ^= Rng << 13; Rng ^= Rng >> 17; Rng ^= Rng << 5; const uint32 R = Rng % 1000;
	const int32 Dist = FMath::Abs(O.x - P.x) / gk::F; const uint16 Fwd = P.face > 0 ? gk::RIGHT : gk::LEFT, Back = P.face > 0 ? gk::LEFT : gk::RIGHT;
	const bool bTick = (State.frame % 2) == 0;   // buttons are tapped, not held
	if (State.phase != gk::PH_FIGHT) { return 0; }
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
	if (State.shot[1 - Me].on && R < 600) { return Back; }
	if (Dist > 420) { return R < 14 ? uint16(gk::BM | gk::BH) : R < 26 ? gk::BS : R < 40 ? uint16(gk::UP | Fwd) : Fwd; }
	if (Dist > 150) { return R < 12 ? uint16(gk::BS | Fwd) : R < 22 ? uint16(gk::BM | gk::BH) : R < 34 ? uint16(gk::UP | Fwd) : R < 44 ? uint16(gk::BS | Back) : Fwd; }
	if (!bTick) { return 0; }
	if (P.y > 0) { return R < 300 ? gk::BH : R < 500 ? gk::BM : 0; }
	return R < 150 ? gk::BL : R < 280 ? gk::BM : R < 380 ? uint16(gk::BM | gk::DOWN) : R < 450 ? gk::BH : R < 500 ? uint16(gk::BH | gk::DOWN) : R < 550 ? uint16(gk::BL | gk::BM) : R < 590 ? uint16(gk::BS | gk::DOWN) : R < 620 ? gk::BP : R < 640 ? uint16(gk::BH | gk::BS) : R < 700 ? Back : 0;
}

void AGkMatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Accumulator += FMath::Min(DeltaSeconds, 0.1f);
	const double Step = 1.0 / 60.0;
	while (Accumulator >= Step)
	{
		Accumulator -= Step;
		const uint16 Human = ReadPlayer();
		IdleSeconds = Human ? 0.0 : IdleSeconds + Step;
		// nobody at the controls for a while: the sparring partner takes both sides, so the game demonstrates itself
		const uint16 In0 = IdleSeconds > 6.0 ? Cpu(0) : Human, In1 = Cpu(1);
		gk::step(State, In0, In1);
		for (int32 e = 0; e < State.nev; e++)
		{
			const gk::Event& E = State.ev[e];
			if (E.type == gk::E_HIT) { Shake = FMath::Max(Shake, 2.f + E.a * 1.6f); Combo[E.who & 1] = E.b; ComboTimer[E.who & 1] = 70; }
			if (E.type == gk::E_GROUNDB || E.type == gk::E_WALLB) { Shake = FMath::Max(Shake, 12.f); }
			if (E.type == gk::E_SUPER) { Shake = FMath::Max(Shake, 6.f); }
		}
		for (int32 i = 0; i < 2; i++) { if (ComboTimer[i] > 0) { ComboTimer[i]--; } if (Fighters[i]) { Fighters[i]->Present(State, State.frame); } }
		Shake *= 0.86f;
		if (State.phase == gk::PH_OVER && State.pt == 0) { State.pt = 1; }
		if (State.phase == gk::PH_OVER) { if (++State.pt > 240) { gk::init(State); } }
	}
	// the camera frames both fighters, pulls in when they close, and rises with them when the fight leaves the ground
	if (Camera)
	{
		const double Y0 = -double(State.p[0].x) / gk::F, Y1 = -double(State.p[1].x) / gk::F, Z = FMath::Max(State.p[0].y, State.p[1].y) / double(gk::F);
		const double Sep = FMath::Abs(Y0 - Y1); const FVector Want(FMath::Clamp(330.0 + Sep * 0.86 + Z * 0.5, 520.0, 1250.0), (Y0 + Y1) * 0.5, 100.0 + Z * 0.42);
		CamPos = FMath::Lerp(CamPos, Want, FMath::Clamp(DeltaSeconds * 6.0, 0.0, 1.0));
		const FVector Sh(0.0, FMath::Sin(State.frame * 1.7) * Shake, FMath::Cos(State.frame * 2.3) * Shake * 0.7);
		Camera->SetActorLocation(CamPos + Sh); Camera->SetActorRotation(FRotator(-1.5, 180.0, 0.0));
	}
}

FVector AGkMatch::FighterLocation(int32 Index) const { const gk::Player& P = State.p[Index & 1]; return FVector(0.0, -double(P.x) / gk::F, double(P.y) / gk::F); }
int32 AGkMatch::FighterHealth(int32 Index) const { return State.p[Index & 1].hp; }

// ------------------------------------------------------------------------------------------------ HUD
void AGkHUD::DrawHUD()
{
	Super::DrawHUD();
	AGkMatch* M = nullptr; TActorIterator<AGkMatch> It(GetWorld()); if (It) { M = *It; }
	if (!M || !Canvas) { return; }
	const gk::State& S = M->State; const float W = Canvas->SizeX, H = Canvas->SizeY, U = H / 1080.f;
	const FLinearColor Ink(0.02f, 0.01f, 0.02f, 0.85f), Bone(0.96f, 0.93f, 0.86f, 1.f), Red(0.95f, 0.12f, 0.1f, 1.f), Pale(0.62f, 0.8f, 1.f, 1.f), Gold(1.f, 0.78f, 0.2f, 1.f);
	const float BarW = 760 * U, BarH = 30 * U, Top = 54 * U, Mid = W * 0.5f, Gap = 96 * U;
	for (int32 i = 0; i < 2; i++)
	{
		const gk::Player& P = S.p[i]; const float K = FMath::Clamp(P.hp / float(gk::MAX_HP), 0.f, 1.f); const FLinearColor C = i ? Pale : Red;
		const float X0 = i ? Mid + Gap : Mid - Gap - BarW;
		DrawRect(Ink, X0 - 5 * U, Top - 5 * U, BarW + 10 * U, BarH + 10 * U);
		// each bar drains toward the middle of the screen
		if (i) { DrawRect(K < 0.25f ? Gold : C, X0, Top, BarW * K, BarH); } else { DrawRect(K < 0.25f ? Gold : C, X0 + BarW * (1.f - K), Top, BarW * K, BarH); }
		DrawText(i ? TEXT("SHIRO") : TEXT("KAGU"), Bone, i ? X0 + BarW - 150 * U : X0, Top + BarH + 12 * U, nullptr, 2.6f * U);
		for (int32 r = 0; r < 2; r++) { DrawRect(r < P.rounds ? Gold : Ink, i ? X0 + r * 34 * U : X0 + BarW - (r + 1) * 34 * U + 8 * U, Top - 30 * U, 24 * U, 14 * U); }
		// the Tide gauge: three bars along the bottom
		const float MW = 150 * U, MY = H - 78 * U;
		for (int32 b = 0; b < 3; b++)
		{
			const float Fill = FMath::Clamp((P.meter - b * gk::BAR) / float(gk::BAR), 0.f, 1.f); const float X = i ? W - 70 * U - (b + 1) * (MW + 10 * U) : 70 * U + b * (MW + 10 * U);
			DrawRect(Ink, X - 3 * U, MY - 3 * U, MW + 6 * U, 22 * U); DrawRect(Fill >= 1.f ? Gold : C, X, MY, MW * Fill, 16 * U);
		}
		if (M->ComboTimer[i] > 0 && M->Combo[i] >= 2)
		{
			DrawText(FString::Printf(TEXT("%d"), M->Combo[i]), Gold, i ? W - 300 * U : 110 * U, 300 * U, nullptr, 9.f * U);
			DrawText(TEXT("HITS"), Bone, i ? W - 300 * U : 110 * U, 430 * U, nullptr, 3.f * U);
		}
	}
	DrawRect(Ink, Mid - 62 * U, Top - 22 * U, 124 * U, 86 * U);
	DrawText(FString::Printf(TEXT("%02d"), FMath::Clamp((S.time + 59) / 60, 0, 99)), Bone, Mid - 44 * U, Top - 14 * U, nullptr, 5.f * U);
	if (S.phase == gk::PH_INTRO) { DrawText(S.pt < 55 ? FString::Printf(TEXT("ROUND %d"), S.round) : FString(TEXT("FIGHT")), Bone, Mid - 230 * U, H * 0.36f, nullptr, 10.f * U); }
	if (S.phase == gk::PH_KO && S.pt < 150) { DrawText(TEXT("K.O."), Red, Mid - 170 * U, H * 0.36f, nullptr, 12.f * U); }
	if (S.phase == gk::PH_OVER) { DrawText(S.winner == 0 ? TEXT("KAGU WINS") : TEXT("SHIRO WINS"), Gold, Mid - 330 * U, H * 0.36f, nullptr, 10.f * U); }
}

// ------------------------------------------------------------------------------------------------ game mode
AGkGameMode::AGkGameMode() { HUDClass = AGkHUD::StaticClass(); DefaultPawnClass = nullptr; }

void AGkGameMode::StartPlay()
{
	Super::StartPlay();
	if (UWorld* W = GetWorld()) { W->SpawnActor<AGkMatch>(FVector::ZeroVector, FRotator::ZeroRotator); }
}
