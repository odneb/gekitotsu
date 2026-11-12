#include "GkPose.h"
#include "ReferenceSkeleton.h"
#include "Misc/FileHelper.h"

// ----------------------------------------------------------------------------------------------- library
FGkLibrary& GkLib() { static FGkLibrary L; return L; }

const FGkPose& FGkLibrary::Pose(FName Name) const
{
	static const FGkPose Fallback;
	if (const FGkPose* P = Poses.Find(Name)) { return *P; }
	return Fallback;
}

const FGkMoveAnim& FGkLibrary::Move(FName Name) const
{
	static const FGkMoveAnim Fallback;
	if (const FGkMoveAnim* M = Moves.Find(Name)) { return *M; }
	return Fallback;
}

/** The file is plain text: "pose Name key numbers key numbers ..." and "move Name key value ...", one per line. */
bool FGkLibrary::Load(const FString& Path, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = FString::Printf(TEXT("cannot read %s"), *Path); return false; }
	TMap<FName, FGkPose> NewPoses; TMap<FName, FGkMoveAnim> NewMoves;
	TArray<FString> Lines; Text.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		TArray<FString> T; Line.ParseIntoArrayWS(T);
		if (T.Num() < 2 || T[0].StartsWith(TEXT("#"))) { continue; }
		auto Num = [&T](int32 i) { return T.IsValidIndex(i) ? FCString::Atof(*T[i]) : 0.f; };
		auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
		if (T[0] == TEXT("pose"))
		{
			FGkPose P;
			for (int32 i = 2; i < T.Num(); )
			{
				const FString& K = T[i]; int32 Used = 2;
				if (K == TEXT("hip")) { P.Hip = Vec(i + 1); Used = 4; }
				else if (K == TEXT("hl")) { P.HL = Vec(i + 1); Used = 4; }
				else if (K == TEXT("hr")) { P.HR = Vec(i + 1); Used = 4; }
				else if (K == TEXT("fl")) { P.FL = Vec(i + 1); Used = 4; }
				else if (K == TEXT("fr")) { P.FR = Vec(i + 1); Used = 4; }
				else if (K == TEXT("el")) { P.EL = Vec(i + 1); Used = 4; }
				else if (K == TEXT("er")) { P.ER = Vec(i + 1); Used = 4; }
				else if (K == TEXT("kl")) { P.KL = Vec(i + 1); Used = 4; }
				else if (K == TEXT("kr")) { P.KR = Vec(i + 1); Used = 4; }
				else if (K == TEXT("tl")) { P.TL = Vec(i + 1); Used = 4; }
				else if (K == TEXT("tr")) { P.TR = Vec(i + 1); Used = 4; }
				else if (K == TEXT("ln")) { P.Ln = Num(i + 1); }
				else if (K == TEXT("ln2")) { P.Ln2 = Num(i + 1); }
				else if (K == TEXT("sd")) { P.Sd = Num(i + 1); }
				else if (K == TEXT("tw")) { P.Tw = Num(i + 1); }
				else if (K == TEXT("pt")) { P.Pt = Num(i + 1); }
				else if (K == TEXT("hd")) { P.Hd = Num(i + 1); }
				else if (K == TEXT("hy")) { P.Hy = Num(i + 1); }
				else if (K == TEXT("roll")) { P.Roll = Num(i + 1); }
				else if (K == TEXT("spin")) { P.Spin = Num(i + 1); }
				i += Used;
			}
			NewPoses.Add(FName(*T[1]), P);
		}
		else if (T[0] == TEXT("move"))
		{
			FGkMoveAnim M;
			for (int32 i = 2; i + 1 < T.Num(); i += 2)
			{
				const FString& K = T[i]; const FString& Val = T[i + 1];
				if (K == TEXT("wind")) { M.Wind = FName(*Val); }
				else if (K == TEXT("strike")) { M.Strike = FName(*Val); }
				else if (K == TEXT("rec")) { if (Val != TEXT("-")) { M.Rec = FName(*Val); } }
				else if (K == TEXT("limb")) { M.Limb = FCString::Atoi(*Val); }
				else if (K == TEXT("arc")) { M.Arc = FCString::Atof(*Val); }
				else if (K == TEXT("size")) { M.ArcSize = FCString::Atof(*Val); }
				else if (K == TEXT("kind")) { M.ArcKind = FCString::Atoi(*Val); }
				else if (K == TEXT("spin")) { M.Spin = FCString::Atof(*Val); }
			}
			NewMoves.Add(FName(*T[1]), M);
		}
	}
	if (NewPoses.Num() == 0) { OutError = TEXT("no poses in file"); return false; }
	Poses = MoveTemp(NewPoses); Moves = MoveTemp(NewMoves); return true;
}

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

/** The same pose seen in a mirror: left and right trade places. Used so a fighter always shows the camera his chest. */
FGkPose GkMirror(const FGkPose& P)
{
	auto Mx = [](const FVector& In) { return FVector(-In.X, In.Y, In.Z); };
	FGkPose R = P; R.Hip = Mx(P.Hip); R.Sd = -P.Sd; R.Tw = -P.Tw; R.Pt = -P.Pt; R.Hy = -P.Hy; R.Spin = -P.Spin;
	R.HL = Mx(P.HR); R.HR = Mx(P.HL); R.FL = Mx(P.FR); R.FR = Mx(P.FL); R.EL = Mx(P.ER); R.ER = Mx(P.EL); R.KL = Mx(P.KR); R.KR = Mx(P.KL); R.TL = Mx(P.TR); R.TR = Mx(P.TL);
	return R;
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
