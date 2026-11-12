#include "GkFx.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Actor.h"

void FGkFx::Init(AActor* Owner, USceneComponent* Root)
{
	Rng.Initialize(7731); Ps.Reset(); Ps.Reserve(Pool * 2);
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	static const TCHAR* Mats[2] = { TEXT("/Game/GkFx/M_GkGlow.M_GkGlow"), TEXT("/Game/GkFx/M_GkDust.M_GkDust") };
	for (int32 L = 0; L < 2; L++)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Mats[L]);
		if (!Plane || !M) { UE_LOG(LogTemp, Warning, TEXT("Gekitotsu: particle material %s did not load"), Mats[L]); Mesh[L] = nullptr; continue; }
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Owner); C->SetMobility(EComponentMobility::Movable); C->SetupAttachment(Root);
		C->SetStaticMesh(Plane); C->SetMaterial(0, M); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false); C->SetNumCustomDataFloats(4);
		C->bNeverDistanceCull = true; C->SetBoundsScale(1000.f); C->TranslucencySortPriority = L == 0 ? 2 : 1; C->RegisterComponent();
		TArray<FTransform> Zero; Zero.Init(FTransform(FQuat::Identity, FVector(0, 0, -10000), FVector::ZeroVector), Pool); C->AddInstances(Zero, false, true);
		Mesh[L] = C; Used[L] = 0;
	}
}

FGkFx::FP& FGkFx::Add(int32 Layer, const FVector& Pos, const FVector& Vel, float Life, float Size0, float Size1, const FLinearColor& Col, float Alpha)
{
	if (Ps.Num() >= Pool * 2 - 4) { Ps.RemoveAt(0, 16, EAllowShrinking::No); }
	FP& P = Ps.AddDefaulted_GetRef(); P.Pos = Pos; P.Vel = Vel; P.Age = 0.f; P.Life = Life; P.Size0 = Size0; P.Size1 = Size1; P.Stretch = 1.f; P.Grav = 0.f; P.Drag = 0.f; P.Alpha = Alpha; P.Col = Col; P.Layer = Layer; P.bSwell = false;
	return P;
}

void FGkFx::Tick(float Dt)
{
	Dt = FMath::Clamp(Dt, 0.f, 0.05f);
	int32 N[2] = { 0, 0 };
	const FQuat Turn(FVector(0, 0, 1), Yaw), Face = Turn * FQuat(FVector(0, 1, 0), HALF_PI);   // the square stood up to face the camera
	for (int32 k = Ps.Num() - 1; k >= 0; k--)
	{
		FP& P = Ps[k]; P.Age += Dt; if (P.Life > 0.f && P.Age >= P.Life) { Ps.RemoveAtSwap(k, 1, EAllowShrinking::No); continue; }
		P.Vel.Z -= P.Grav * Dt; P.Vel *= FMath::Max(0.f, 1.f - P.Drag * Dt); P.Pos += P.Vel * Dt;
		if (P.Grav > 0.f && P.Pos.Z < 2.0) { P.Pos.Z = 2.0; P.Vel.Z *= -0.35; P.Vel.Y *= 0.6; }
	}
	for (const FP& P : Ps)
	{
		UInstancedStaticMeshComponent* C = Mesh[P.Layer & 1]; int32& n = N[P.Layer & 1]; if (!C || n >= Pool) { continue; }
		const float t = P.Life > 0.f ? P.Age / P.Life : 0.f, Size = FMath::Lerp(P.Size0, P.Size1, t);
		const float Fade = P.Life <= 0.f ? 1.f : P.bSwell ? FMath::Sin(t * PI) : (1.f - t) * (1.f - t);
		FQuat Q = Face; FVector Sc(Size / 100.f, Size / 100.f, 1.f);
		if (P.Stretch > 1.f)
		{
			const double Sp = FMath::Sqrt(P.Vel.Y * P.Vel.Y + P.Vel.Z * P.Vel.Z);
			Q = Turn * FQuat(FVector(1, 0, 0), FMath::Atan2(P.Vel.Z, P.Vel.Y)) * FQuat(FVector(0, 1, 0), HALF_PI); Sc.Y *= 1.f + (P.Stretch - 1.f) * FMath::Clamp(float(Sp) / 600.f, 0.f, 1.5f); Sc.X *= 0.5f;
		}
		C->UpdateInstanceTransform(n, FTransform(Q, P.Pos, Sc), true, false, true);
		const float Data[4] = { P.Col.R, P.Col.G, P.Col.B, P.Alpha * Fade }; C->SetCustomData(n, MakeArrayView(Data, 4), false);
		n++;
	}
	for (int32 L = 0; L < 2; L++)
	{
		if (!Mesh[L]) { continue; }
		for (int32 i = N[L]; i < Used[L]; i++) { Mesh[L]->UpdateInstanceTransform(i, FTransform(FQuat::Identity, FVector(0, 0, -10000), FVector::ZeroVector), true, false, true); }
		Used[L] = N[L]; Mesh[L]->MarkRenderStateDirty();
	}
}

void FGkFx::Hit(const FVector& At, int32 Level, float Dir)
{
	Level = FMath::Clamp(Level, 0, 3);
	static const FLinearColor Core[4] = { FLinearColor(6, 5, 3), FLinearColor(8, 5, 2), FLinearColor(10, 4, 1.2f), FLinearColor(14, 6, 2) };
	static const FLinearColor Ember[4] = { FLinearColor(5, 3.5f, 1), FLinearColor(6, 2.6f, 0.6f), FLinearColor(7, 1.8f, 0.4f), FLinearColor(9, 3, 0.6f) };
	const FVector P = At + FVector(-25, 0, 0); const float Big = 1.f + Level * 0.45f;
	Add(0, P, FVector::ZeroVector, 0.10f + Level * 0.02f, 55.f * Big, 130.f * Big, Core[Level] * 0.8f);
	Add(0, P, FVector::ZeroVector, 0.2f + Level * 0.04f, 40.f * Big, 230.f * Big, Ember[Level] * 0.18f);
	{ FP& Slash = Add(0, P, FVector(0, Dir * 900.f, 250.f), 0.12f, 60.f * Big, 20.f, Core[Level]); Slash.Stretch = 9.f; }
	{ FP& Slash = Add(0, P, FVector(0, Dir * 700.f, -500.f), 0.12f, 50.f * Big, 20.f, Core[Level]); Slash.Stretch = 8.f; }
	const int32 Count = 10 + Level * 7;
	for (int32 i = 0; i < Count; i++)
	{
		const float A = Rng.FRandRange(-1.25f, 1.25f), Sp = Rng.FRandRange(450.f, 1300.f) * (0.8f + Level * 0.2f);
		FP& S = Add(0, P, FVector(Rng.FRandRange(-80.f, 40.f), Dir * FMath::Cos(A) * Sp, FMath::Sin(A) * Sp + 120.f), Rng.FRandRange(0.18f, 0.42f), Rng.FRandRange(5.f, 10.f), 2.f, Ember[Level]);
		S.Stretch = 7.f; S.Grav = 900.f; S.Drag = 3.5f;
	}
	if (Level >= 2) { Dust(FVector(At.X, At.Y, 0), 0.6f + Level * 0.3f, Dir); }
}

void FGkFx::Guard(const FVector& At, float Dir)
{
	const FVector P = At + FVector(-25, 0, 0);
	Add(0, P, FVector::ZeroVector, 0.12f, 50.f, 130.f, FLinearColor(1.5f, 3.5f, 7));
	for (int32 i = 0; i < 7; i++)
	{
		const float A = Rng.FRandRange(-1.4f, 1.4f), Sp = Rng.FRandRange(300.f, 700.f);
		FP& S = Add(0, P, FVector(0, -Dir * FMath::Cos(A) * Sp, FMath::Sin(A) * Sp), Rng.FRandRange(0.12f, 0.25f), 6.f, 2.f, FLinearColor(1.2f, 2.8f, 6)); S.Stretch = 6.f; S.Drag = 5.f;
	}
}

void FGkFx::Parry(const FVector& At, bool bPerfect)
{
	const FVector P = At + FVector(-25, 0, 0); const FLinearColor C = bPerfect ? FLinearColor(3, 8, 9) : FLinearColor(1.5f, 5, 6);
	Add(0, P, FVector::ZeroVector, 0.16f, 60.f, bPerfect ? 300.f : 190.f, C);
	const int32 Count = bPerfect ? 22 : 12;
	for (int32 i = 0; i < Count; i++)
	{
		const float A = (i + Rng.FRand() * 0.5f) * 2.f * PI / Count, Sp = bPerfect ? 1100.f : 750.f;
		FP& S = Add(0, P, FVector(0, FMath::Cos(A) * Sp, FMath::Sin(A) * Sp), 0.2f, 7.f, 2.f, C); S.Stretch = 8.f; S.Drag = 6.f;
	}
}

void FGkFx::Dust(const FVector& At, float Amount, float Dir)
{
	const int32 Count = FMath::Clamp(int32(3 + Amount * 5.f), 3, 14);
	for (int32 i = 0; i < Count; i++)
	{
		const float Side = Dir != 0.f ? -Dir : (Rng.FRand() < 0.5f ? -1.f : 1.f);
		FP& D = Add(1, FVector(At.X + Rng.FRandRange(-30.f, 20.f), At.Y + Rng.FRandRange(-18.f, 18.f), Rng.FRandRange(4.f, 16.f)),
			FVector(Rng.FRandRange(-30.f, 30.f), Side * Rng.FRandRange(40.f, 240.f) * Amount, Rng.FRandRange(15.f, 110.f) * Amount),
			Rng.FRandRange(0.45f, 0.95f), Rng.FRandRange(18.f, 34.f), Rng.FRandRange(70.f, 130.f) * FMath::Clamp(Amount, 0.6f, 1.6f), DustCol, 0.42f);
		D.Drag = 2.6f;
	}
}

void FGkFx::Aura(const FVector& At, const FLinearColor& Col, float Amount)
{
	Add(0, At + FVector(-20, 0, 95), FVector::ZeroVector, 0.3f, 120.f, 420.f * Amount, Col * 0.6f);
	const int32 Count = int32(14 * Amount);
	for (int32 i = 0; i < Count; i++)
	{
		FP& S = Add(0, At + FVector(Rng.FRandRange(-40.f, 20.f), Rng.FRandRange(-45.f, 45.f), Rng.FRandRange(0.f, 150.f)), FVector(0, Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(180.f, 520.f)), Rng.FRandRange(0.3f, 0.7f), Rng.FRandRange(8.f, 16.f), 3.f, Col);
		S.Stretch = 4.f;
	}
	Dust(FVector(At.X, At.Y, 0), 0.8f * Amount, 0.f);
}

void FGkFx::Trail(const FVector& At, const FLinearColor& Col)
{
	Add(0, At + FVector(-10, 0, 0), FVector::ZeroVector, 0.22f, 95.f, 20.f, Col);
	FP& S = Add(0, At, FVector(0, Rng.FRandRange(-120.f, 120.f), Rng.FRandRange(-120.f, 120.f)), 0.3f, 7.f, 2.f, Col * 1.5f); S.Drag = 2.f;
}

void FGkFx::Air(double CentreY, float Dt, bool bOutdoors)
{
	MoteDue -= Dt; HazeDue -= Dt;
	while (MoteDue <= 0.f)
	{
		MoteDue += bOutdoors ? 0.07f : 0.2f;
		FP& M = Add(0, FVector(Rng.FRandRange(-420.f, 160.f), CentreY + Rng.FRandRange(-900.f, 900.f), Rng.FRandRange(5.f, 380.f)), FVector(0, Rng.FRandRange(18.f, 60.f), Rng.FRandRange(-10.f, 14.f)),
			Rng.FRandRange(3.5f, 7.f), Rng.FRandRange(2.5f, 6.f), Rng.FRandRange(2.5f, 6.f), bOutdoors ? FLinearColor(2.4f, 1.7f, 0.9f) : FLinearColor(0.9f, 1.1f, 1.5f), 0.55f);
		M.bSwell = true;
	}
	while (HazeDue <= 0.f && bOutdoors)
	{
		HazeDue += 0.45f;
		FP& H = Add(1, FVector(Rng.FRandRange(-350.f, 110.f), CentreY + Rng.FRandRange(-1100.f, 900.f), Rng.FRandRange(8.f, 40.f)), FVector(0, Rng.FRandRange(50.f, 130.f), Rng.FRandRange(0.f, 8.f)),
			Rng.FRandRange(4.f, 7.f), Rng.FRandRange(160.f, 260.f), Rng.FRandRange(280.f, 420.f), DustCol, 0.1f);
		H.bSwell = true;
	}
	if (HazeDue < -1.f) { HazeDue = 0.f; }
}
