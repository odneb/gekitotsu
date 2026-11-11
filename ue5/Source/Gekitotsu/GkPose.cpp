#include "GkPose.h"
#include "ReferenceSkeleton.h"

static FGkPose Mk(FVector Hip, float Ln, float Ln2, float Tw, float Pt, float Hd, float Hy, FVector HL, FVector HR, FVector FL, FVector FR)
{
	FGkPose P; P.Hip = Hip; P.Ln = Ln; P.Ln2 = Ln2; P.Tw = Tw; P.Pt = Pt; P.Hd = Hd; P.Hy = Hy; P.HL = HL; P.HR = HR; P.FL = FL; P.FR = FR; return P;
}
static FGkPose With(FGkPose P, FVector EL, FVector ER) { P.EL = EL; P.ER = ER; return P; }
static FGkPose Knees(FGkPose P, FVector KL, FVector KR) { P.KL = KL; P.KR = KR; return P; }
typedef FVector V;

static TArray<FGkPose> BuildPoses()
{
	TArray<FGkPose> A; A.SetNum((int32)EGkPose::Count);
	auto S = [&A](EGkPose E, const FGkPose& P) { A[(int32)E] = P; };
	const V AB(0.4, -1, 0.4), ABr(-0.4, -1, 0.4);   // elbows carried back
	// standing: a reed in the wind. Weight on the back leg, lead hand offered open, the other trailing low behind.
	S(EGkPose::Idle, FGkPose());
	S(EGkPose::Idle2, Mk(V(0, -3, 81), 2, 10, -48, -32, -2, 40, V(15, 44, 124), V(-24, -14, 84), V(9, 26, 9), V(-13, -27, 9)));
	S(EGkPose::Crouch, Knees(Mk(V(0, -8, 50), 30, 40, -40, -26, -30, 30, V(12, 40, 70), V(-22, -20, 56), V(11, 30, 9), V(-15, -34, 11)), V(0.7, 1, 0.3), V(-0.9, 0.5, 0.2)));
	S(EGkPose::WalkA, Mk(V(0, 0, 84), 2, 6, -46, -30, 2, 36, V(14, 44, 128), V(-24, -12, 84), V(9, 34, 9), V(-12, -10, 16)));
	S(EGkPose::WalkB, Mk(V(0, 2, 82), 4, 8, -46, -30, 0, 36, V(15, 42, 126), V(-24, -16, 82), V(9, 4, 15), V(-13, -30, 9)));
	S(EGkPose::Jump, Mk(V(0, 0, 95), 24, 34, -30, -20, -20, 20, V(14, 26, 104), V(-18, 10, 98), V(10, 24, 58), V(-10, 2, 50)));
	S(EGkPose::Fall, With(Mk(V(0, 0, 95), -12, -18, -40, -30, 12, 30, V(34, 10, 140), V(-36, -12, 134), V(10, 14, 24), V(-12, -16, 30)), V(1, 0, -0.3), V(-1, 0, -0.3)));
	// the dash is a ninja's: body thrown forward, both arms streaming behind
	S(EGkPose::Dash, With(Mk(V(0, 10, 70), 46, 54, -24, -14, -40, 14, V(16, -22, 96), V(-20, -34, 92), V(9, 40, 9), V(-12, -36, 22)), AB, ABr));
	S(EGkPose::BackDash, Mk(V(0, -14, 82), -26, -32, -60, -40, 22, 40, V(20, 30, 120), V(-26, -6, 100), V(9, 22, 14), V(-12, -30, 9)));
	S(EGkPose::BlockS, With(Mk(V(0, -10, 84), -4, 4, -30, -26, 2, 24, V(6, 30, 128), V(-8, 26, 108), V(9, 22, 9), V(-13, -28, 9)), V(0.6, -0.3, -1), V(-0.6, -0.3, -1)));
	S(EGkPose::BlockC, With(Knees(Mk(V(0, -8, 50), 22, 30, -30, -22, -22, 24, V(6, 30, 84), V(-8, 26, 70), V(11, 28, 9), V(-15, -32, 11)), V(0.7, 1, 0.3), V(-0.9, 0.5, 0.2)), V(0.6, -0.3, -1), V(-0.6, -0.3, -1)));
	S(EGkPose::Hit, Mk(V(0, -14, 80), -22, -34, -60, -40, -20, 30, V(24, 10, 124), V(-28, -20, 104), V(9, 20, 9), V(-13, -32, 9)));
	S(EGkPose::Launch, Mk(V(0, 0, 60), -40, -56, -50, -30, -26, 20, V(30, -10, 90), V(-34, -30, 70), V(12, 34, 70), V(-10, 20, 44)));
	S(EGkPose::Down, Mk(V(0, -10, 14), -84, -90, -20, -10, -10, 0, V(34, -40, 10), V(-36, -60, 10), V(12, 60, 12), V(-12, 50, 26)));
	S(EGkPose::Rise, Knees(Mk(V(0, -6, 46), 34, 44, -30, -20, -30, 20, V(14, 34, 10), V(-20, -10, 60), V(11, 26, 9), V(-15, -34, 11)), V(0.7, 1, 0.3), V(-0.9, 0.5, 0.2)));
	S(EGkPose::Parry, With(Mk(V(0, -6, 82), -2, 2, -66, -38, 2, 54, V(4, 40, 120), V(-22, -10, 96), V(9, 24, 9), V(-13, -28, 9)), V(0.8, 0, -0.6), V(-0.8, -0.6, 0.2)));
	// grounded strikes: thrown while swaying away or toppling in
	S(EGkPose::LS, Mk(V(0, -2, 84), -4, 0, -70, -40, 4, 60, V(6, 80, 130), V(-24, -16, 84), V(9, 26, 9), V(-13, -27, 9)));
	S(EGkPose::MW, Mk(V(0, -10, 82), -12, -8, -76, -44, 6, 66, V(10, 30, 120), V(-22, -10, 112), V(9, 24, 9), V(-13, -28, 9)));
	S(EGkPose::MS, With(Mk(V(0, 14, 74), 30, 36, 28, 6, -26, -20, V(22, -10, 96), V(-4, 60, 116), V(9, 40, 9), V(-13, -34, 14)), V(0.6, -1, 0), V(-0.5, -0.3, -1)));
	S(EGkPose::HW, With(Mk(V(0, -12, 82), -18, -26, -84, -50, 10, 70, V(12, 34, 116), V(-30, -24, 150), V(9, 24, 10), V(-13, -28, 9)), V(0.3, -0.2, -1), V(-1, -0.4, 0.4)));
	S(EGkPose::HS, With(Mk(V(0, 16, 72), 34, 42, 46, 14, -28, -34, V(26, -16, 84), V(-2, 66, 112), V(9, 44, 9), V(-13, -36, 16)), V(0.6, -1, 0), V(-0.5, -0.2, 1)));
	S(EGkPose::CL, Knees(Mk(V(0, -10, 48), 10, 14, -50, -36, -10, 40, V(14, 20, 62), V(-22, -22, 40), V(8, 70, 14), V(-15, -30, 11)), V(0.3, 1, 0.6), V(-0.9, 0.5, 0.2)));
	S(EGkPose::CM, Knees(Mk(V(0, -4, 30), -36, -30, -60, -44, 30, 50, V(16, -6, 50), V(-24, -34, 14), V(6, 88, 14), V(-16, -10, 12)), V(0.3, 1, 0.6), V(-1, 0.4, 0.3)));
	S(EGkPose::CHW, Knees(Mk(V(0, -4, 44), 36, 46, -60, -40, -30, 50, V(14, 30, 70), V(-10, 10, 40), V(11, 28, 9), V(-15, -32, 11)), V(0.7, 1, 0.3), V(-0.9, 0.5, 0.2)));
	S(EGkPose::CHS, With(Mk(V(0, 8, 88), -10, -16, 40, 10, -18, -30, V(22, -14, 96), V(-6, 36, 186), V(9, 26, 12), V(-13, -24, 9)), V(0.6, -1, 0), V(-0.6, 0.4, -0.4)));
	// in the air
	S(EGkPose::JL, Mk(V(0, 0, 95), 20, 26, -60, -40, -12, 50, V(6, 50, 96), V(-20, 0, 100), V(10, 20, 56), V(-10, -4, 48)));
	S(EGkPose::JM, Knees(Mk(V(0, 0, 95), -20, -16, -70, -60, 14, 60, V(20, 10, 120), V(-26, -20, 110), V(6, 74, 70), V(-10, -10, 52)), V(0.4, 0.6, 1), V(-0.9, 0.6, 0)));
	S(EGkPose::JH, Knees(Mk(V(0, 0, 95), -16, -12, 60, 70, 10, -50, V(26, -10, 118), V(-22, -26, 112), V(10, -10, 54), V(-4, 76, 70)), V(0.5, 1, 0), V(-0.4, 0.6, 1)));
	S(EGkPose::JD, With(Mk(V(0, 0, 100), 16, 20, -40, -30, -14, 30, V(30, 10, 142), V(-32, -10, 138), V(6, 40, 30), V(-10, -14, 74)), V(1, 0, -0.2), V(-1, 0, -0.2)));
	// specials
	S(EGkPose::FireW, Mk(V(0, -12, 80), -8, -4, -80, -50, 4, 70, V(4, -4, 96), V(-14, -10, 92), V(9, 24, 9), V(-13, -28, 9)));
	S(EGkPose::FireS, With(Mk(V(0, 12, 72), 22, 26, -70, -30, -18, 60, V(6, 62, 112), V(-28, -40, 104), V(9, 46, 9), V(-13, -38, 12)), V(0.3, -0.2, -1), V(-0.6, -1, 0.3)));
	S(EGkPose::SurgeW, Mk(V(0, 4, 86), 4, 6, 30, 30, 0, -20, V(18, 20, 118), V(-22, 6, 110), V(9, 6, 9), V(-6, 10, 40)));
	S(EGkPose::SurgeS, Knees(Mk(V(0, 18, 86), -28, -24, 60, 64, 16, -50, V(24, -12, 116), V(-26, -24, 104), V(9, 6, 9), V(-2, 96, 100)), V(0.5, 1, 0), V(-0.4, 0.5, 1)));
	S(EGkPose::RiseS, Knees(With(Mk(V(0, 4, 96), -10, -16, 70, 40, -16, -50, V(24, -10, 100), V(-4, 26, 190), V(10, 26, 40), V(-10, 0, 14)), V(0.6, -1, 0), V(-0.6, 0.4, -0.4)), V(0.3, 1, 0.8), V(-0.9, 0.6, 0)));
	S(EGkPose::Vanish, With(Mk(V(0, 0, 84), 0, 0, -88, -60, 4, 80, V(8, 12, 112), V(-8, 10, 104), V(8, 10, 9), V(-10, -10, 9)), V(0.9, -0.2, -0.6), V(-0.9, -0.2, -0.6)));
	S(EGkPose::Whirl, Knees(With(Mk(V(0, 0, 95), -8, -6, -20, -10, 6, 10, V(44, 6, 128), V(-44, -6, 124), V(60, 0, 76), V(-8, 0, 44)), V(1, 0, 0.2), V(-1, 0, 0.2)), V(0.6, 0.2, 1), V(-0.6, 1, 0)));
	S(EGkPose::Dive, Knees(Mk(V(0, 0, 100), -30, -22, -80, -70, 22, 70, V(26, -16, 146), V(-30, -30, 130), V(4, 60, 34), V(-10, -16, 96)), V(0.3, 1, 0.4), V(-0.6, 1, 0.4)));
	// Slipstream: coil, then a dart with one hand speared ahead and the legs trailing
	S(EGkPose::HomeW, With(Mk(V(0, -6, 56), 50, 58, -40, -26, -40, 30, V(16, -20, 80), V(-20, -30, 74), V(10, 24, 9), V(-13, -26, 10)), AB, ABr));
	S(EGkPose::HomeS, Knees(With(Mk(V(0, 0, 90), 60, 66, -30, -20, -62, 20, V(8, 60, 150), V(-22, -40, 96), V(8, -30, 60), V(-10, -50, 54)), V(0.3, -0.2, -1), ABr), V(0.3, 0.4, -1), V(-0.3, 0.4, -1)));
	S(EGkPose::Grab, With(Mk(V(0, 6, 80), 14, 18, -30, -20, -8, 20, V(10, 52, 124), V(-10, 50, 112), V(9, 34, 9), V(-13, -28, 9)), V(0.6, -0.3, -1), V(-0.6, -0.3, -1)));
	S(EGkPose::Held, Mk(V(0, 0, 84), -14, -22, -40, -30, -20, 20, V(22, -6, 100), V(-24, -16, 92), V(9, 10, 12), V(-12, -14, 10)));
	S(EGkPose::SuperS, With(Mk(V(0, 20, 70), 40, 46, 40, 12, -34, -30, V(26, -30, 90), V(-2, 70, 112), V(9, 50, 9), V(-13, -40, 18)), AB, V(-0.5, -0.2, 1)));
	S(EGkPose::Win, With(Mk(V(0, 0, 90), -4, -2, -90, -80, 8, 80, V(10, 10, 108), V(-12, 6, 102), V(8, 8, 9), V(-9, -8, 9)), V(0.9, 0, -0.4), V(-0.9, 0, -0.4)));
	return A;
}

const FGkPose& GkPose(EGkPose P) { static const TArray<FGkPose> A = BuildPoses(); return A[(int32)P]; }

FGkPose GkBlend(const FGkPose& A, const FGkPose& B, float K)
{
	if (K <= 0.f) { return A; }
	if (K >= 1.f) { return B; }
	FGkPose R; auto L = [K](float a, float b) { return a + (b - a) * K; };
	R.Hip = FMath::Lerp(A.Hip, B.Hip, K); R.Ln = L(A.Ln, B.Ln); R.Ln2 = L(A.Ln2, B.Ln2); R.Sd = L(A.Sd, B.Sd); R.Tw = L(A.Tw, B.Tw); R.Pt = L(A.Pt, B.Pt); R.Hd = L(A.Hd, B.Hd); R.Hy = L(A.Hy, B.Hy);
	R.HL = FMath::Lerp(A.HL, B.HL, K); R.HR = FMath::Lerp(A.HR, B.HR, K); R.FL = FMath::Lerp(A.FL, B.FL, K); R.FR = FMath::Lerp(A.FR, B.FR, K);
	R.EL = FMath::Lerp(A.EL, B.EL, K); R.ER = FMath::Lerp(A.ER, B.ER, K); R.KL = FMath::Lerp(A.KL, B.KL, K); R.KR = FMath::Lerp(A.KR, B.KR, K); R.TL = FMath::Lerp(A.TL, B.TL, K); R.TR = FMath::Lerp(A.TR, B.TR, K);
	R.Roll = L(A.Roll, B.Roll); R.Spin = L(A.Spin, B.Spin); return R;
}

// ----------------------------------------------------------------------------------------------- solving
void FGkRig::Init(const FReferenceSkeleton& Skel)
{
	const int32 N = Skel.GetNum(); Names.SetNum(N); Parent.SetNum(N); RefLocal = Skel.GetRefBonePose(); RefCS.SetNum(N); Index.Reset();
	for (int32 i = 0; i < N; i++)
	{
		Names[i] = Skel.GetBoneName(i); Parent[i] = Skel.GetParentIndex(i); Index.Add(Names[i], i);
		RefCS[i] = Parent[i] >= 0 ? RefLocal[i] * RefCS[Parent[i]] : RefLocal[i];
	}
}

static FQuat Frame(const FVector& Dir, const FVector& Hint) { return FRotationMatrix::MakeFromXZ(Dir.GetSafeNormal(), Hint.GetSafeNormal()).ToQuat(); }
static FVector RotZ(const FVector& Vv, float Deg) { const float A = FMath::DegreesToRadians(Deg), C = FMath::Cos(A), Sn = FMath::Sin(A); return FVector(Vv.X * C - Vv.Y * Sn, Vv.X * Sn + Vv.Y * C, Vv.Z); }
static void IK2(const FVector& A, const FVector& T, double L1, double L2, const FVector& Pole, FVector& Mid, FVector& End)
{
	FVector D = T - A; const double Len = FMath::Clamp(D.Size(), 1e-3, (L1 + L2) * 0.999); const FVector Dn = D.GetSafeNormal(1e-8, FVector(0, 0, -1));
	const double X = (L1 * L1 - L2 * L2 + Len * Len) / (2 * Len), H = FMath::Sqrt(FMath::Max(0.0, L1 * L1 - X * X));
	FVector Side = Pole - Dn * FVector::DotProduct(Pole, Dn); Side = Side.SizeSquared() > 1e-8 ? Side.GetSafeNormal() : FVector(0, 0, -1);
	Mid = A + Dn * X + Side * H; End = A + Dn * Len;
}
static FVector Lean(float Deg, float SideDeg, const FVector& Fw, const FVector& Lf)
{
	const float A = FMath::DegreesToRadians(Deg), B = FMath::DegreesToRadians(SideDeg);
	return (Fw * FMath::Sin(A) + Lf * FMath::Sin(B) + FVector(0, 0, FMath::Cos(A) * FMath::Cos(B))).GetSafeNormal();
}

void FGkRig::Solve(const FGkPose& P, TArray<FTransform>& Out) const
{
	const int32 N = Names.Num(); Out.SetNum(N); if (!IsValid()) { Out = RefCS; return; }
	auto I = [this](const TCHAR* Nm) { const int32* F = Index.Find(FName(Nm)); return F ? *F : 0; };
	auto RP = [this, &I](const TCHAR* Nm) { return RefCS[I(Nm)].GetLocation(); };
	auto Seg = [&RP](const TCHAR* A, const TCHAR* B) { return (RP(B) - RP(A)).Size(); };
	TMap<int32, FVector> J; TArray<FVector> Hint; Hint.Init(FVector(0, 1, 0), N);
	const FVector Fwd = RotZ(FVector(0, 1, 0), P.Pt), Cf = RotZ(FVector(0, 1, 0), P.Tw), Cl = RotZ(FVector(1, 0, 0), P.Tw), Pl = RotZ(FVector(1, 0, 0), P.Pt);
	const FVector D1 = Lean(P.Ln * 0.5f, P.Sd * 0.5f, Fwd, Pl), D2 = Lean(P.Ln, P.Sd, Cf, Cl), D3 = Lean(P.Ln2, P.Sd, Cf, Cl), Hf = RotZ(Cf, P.Hy), Dh = Lean(P.Ln2 * 0.4f + P.Hd, 0, Hf, RotZ(Cl, P.Hy));
	const FVector Hips = P.Hip; J.Add(I(TEXT("Hips")), Hips);
	const FVector S2 = Hips + D1 * Seg(TEXT("Hips"), TEXT("Spine02")), S1 = S2 + D2 * Seg(TEXT("Spine02"), TEXT("Spine01")), Sp = S1 + D3 * Seg(TEXT("Spine01"), TEXT("Spine")), Nk = Sp + D3 * Seg(TEXT("Spine"), TEXT("neck")), Hd = Nk + Dh * Seg(TEXT("neck"), TEXT("Head"));
	J.Add(I(TEXT("Spine02")), S2); J.Add(I(TEXT("Spine01")), S1); J.Add(I(TEXT("Spine")), Sp); J.Add(I(TEXT("neck")), Nk); J.Add(I(TEXT("Head")), Hd);
	Hint[I(TEXT("Hips"))] = Fwd; Hint[I(TEXT("Spine02"))] = Fwd; Hint[I(TEXT("Spine01"))] = Cf; Hint[I(TEXT("Spine"))] = Cf; Hint[I(TEXT("neck"))] = Hf; Hint[I(TEXT("Head"))] = Hf;
	FVector ChestL = FVector::CrossProduct(D3, Cf).GetSafeNormal(); if (FVector::DotProduct(ChestL, Cl) < 0) ChestL = -ChestL;
	FVector PelvL = FVector::CrossProduct(D1, Fwd).GetSafeNormal(); if (FVector::DotProduct(PelvL, Pl) < 0) PelvL = -PelvL;
	for (int32 Side = 0; Side < 2; Side++)
	{
		const bool bL = Side == 0; const double Sg = bL ? 1.0 : -1.0;
		const TCHAR* Sh = bL ? TEXT("LeftShoulder") : TEXT("RightShoulder"); const TCHAR* Ar = bL ? TEXT("LeftArm") : TEXT("RightArm"); const TCHAR* Fa = bL ? TEXT("LeftForeArm") : TEXT("RightForeArm"); const TCHAR* Ha = bL ? TEXT("LeftHand") : TEXT("RightHand");
		const TCHAR* Ul = bL ? TEXT("LeftUpLeg") : TEXT("RightUpLeg"); const TCHAR* Lg = bL ? TEXT("LeftLeg") : TEXT("RightLeg"); const TCHAR* Ft = bL ? TEXT("LeftFoot") : TEXT("RightFoot"); const TCHAR* To = bL ? TEXT("LeftToeBase") : TEXT("RightToeBase");
		const FVector Ro = RP(Ar) - RP(TEXT("Spine")), Rs = RP(Sh) - RP(TEXT("Spine"));
		const FVector ShP = Sp + ChestL * FMath::Abs(Ro.X) * Sg + D3 * Ro.Z + Cf * Ro.Y, ClP = Sp + ChestL * FMath::Abs(Rs.X) * Sg + D3 * Rs.Z + Cf * Rs.Y;
		FVector El, Wr; IK2(ShP, bL ? P.HL : P.HR, Seg(Ar, Fa), Seg(Fa, Ha), bL ? P.EL : P.ER, El, Wr);
		J.Add(I(Sh), ClP); J.Add(I(Ar), ShP); J.Add(I(Fa), El); J.Add(I(Ha), Wr);
		FVector ArmN = FVector::CrossProduct(El - ShP, Wr - El); if (ArmN.Size() < 0.5) ArmN = FVector(0, 0, 1);
		Hint[I(Sh)] = Cf; Hint[I(Ar)] = ArmN; Hint[I(Fa)] = ArmN;
		const FVector Rh = RP(Ul) - RP(TEXT("Hips")); const FVector HipJ = Hips + PelvL * FMath::Abs(Rh.X) * Sg + D1 * Rh.Z + Fwd * Rh.Y;
		FVector Kn, An; IK2(HipJ, bL ? P.FL : P.FR, Seg(Ul, Lg), Seg(Lg, Ft), bL ? P.KL : P.KR, Kn, An);
		J.Add(I(Ul), HipJ); J.Add(I(Lg), Kn); J.Add(I(Ft), An); J.Add(I(To), An + (bL ? P.TL : P.TR).GetSafeNormal() * Seg(Ft, To));
		FVector LegN = FVector::CrossProduct(Kn - HipJ, An - Kn); if (LegN.Size() < 0.5) LegN = FVector(1, 0, 0);
		Hint[I(Ul)] = LegN; Hint[I(Lg)] = LegN; Hint[I(Ft)] = FVector(0, 0, 1);
	}
	// reference hints, built the same way from the bind pose
	auto RefHint = [this, &RP](int32 b) -> FVector
	{
		const FString Nm = Names[b].ToString(); const bool bL = Nm.StartsWith(TEXT("Left"));
		if (Nm.Contains(TEXT("Arm")) && !Nm.Contains(TEXT("Shoulder")) ) { const FVector A = RP(bL ? TEXT("LeftArm") : TEXT("RightArm")), B = RP(bL ? TEXT("LeftForeArm") : TEXT("RightForeArm")), C = RP(bL ? TEXT("LeftHand") : TEXT("RightHand")); FVector X = FVector::CrossProduct(B - A, C - B); return X.Size() > 0.5 ? X : FVector(0, 0, 1); }
		if (Nm.EndsWith(TEXT("Foot"))) return FVector(0, 0, 1);
		if (Nm.Contains(TEXT("Leg"))) { const FVector A = RP(bL ? TEXT("LeftUpLeg") : TEXT("RightUpLeg")), B = RP(bL ? TEXT("LeftLeg") : TEXT("RightLeg")), C = RP(bL ? TEXT("LeftFoot") : TEXT("RightFoot")); FVector X = FVector::CrossProduct(B - A, C - B); return X.Size() > 0.5 ? X : FVector(1, 0, 0); }
		return FVector(0, 1, 0);
	};
	// the child each bone points at
	static const TCHAR* Pairs[][2] = { { TEXT("Hips"), TEXT("Spine02") }, { TEXT("Spine02"), TEXT("Spine01") }, { TEXT("Spine01"), TEXT("Spine") }, { TEXT("Spine"), TEXT("neck") }, { TEXT("neck"), TEXT("Head") },
		{ TEXT("LeftShoulder"), TEXT("LeftArm") }, { TEXT("LeftArm"), TEXT("LeftForeArm") }, { TEXT("LeftForeArm"), TEXT("LeftHand") }, { TEXT("RightShoulder"), TEXT("RightArm") }, { TEXT("RightArm"), TEXT("RightForeArm") }, { TEXT("RightForeArm"), TEXT("RightHand") },
		{ TEXT("LeftUpLeg"), TEXT("LeftLeg") }, { TEXT("LeftLeg"), TEXT("LeftFoot") }, { TEXT("LeftFoot"), TEXT("LeftToeBase") }, { TEXT("RightUpLeg"), TEXT("RightLeg") }, { TEXT("RightLeg"), TEXT("RightFoot") }, { TEXT("RightFoot"), TEXT("RightToeBase") } };
	TArray<int32> Child; Child.Init(-1, N); for (const auto& Pr : Pairs) Child[I(Pr[0])] = I(Pr[1]);
	for (int32 b = 0; b < N; b++)
	{
		const int32 Par = Parent[b]; FQuat Q; FVector Pos;
		if (Child[b] >= 0 && J.Contains(b) && J.Contains(Child[b]))
		{
			const FQuat Qt = Frame(J[Child[b]] - J[b], Hint[b]), Qb = Frame(RefCS[Child[b]].GetLocation() - RefCS[b].GetLocation(), RefHint(b));
			Q = Qt * Qb.Inverse() * RefCS[b].GetRotation(); Pos = J[b];
		}
		else if (Par >= 0) { Q = Out[Par].GetRotation() * RefLocal[b].GetRotation(); Pos = J.Contains(b) ? J[b] : Out[Par].GetLocation() + Out[Par].GetRotation().RotateVector(RefLocal[b].GetLocation()); }
		else { Q = RefCS[b].GetRotation(); Pos = J.Contains(b) ? J[b] : RefCS[b].GetLocation(); }
		Out[b] = FTransform(Q.GetNormalized(), Pos, FVector::OneVector);
	}
}
