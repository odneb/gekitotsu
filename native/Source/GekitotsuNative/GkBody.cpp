#include "GkBody.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "ReferenceSkeleton.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FGkPoses::Load(const FString& Dir)
{
	Clips.Reset(); Data.Reset();
	FString Text; TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToString(Text, *(Dir / TEXT("poses.txt"))) || !FFileHelper::LoadFileToArray(Bytes, *(Dir / TEXT("poses.bin")))) { return false; }
	Data.SetNumUninitialized(Bytes.Num() / 4); FMemory::Memcpy(Data.GetData(), Bytes.GetData(), Data.Num() * 4);
	TArray<FString> Lines; Text.ParseIntoArrayLines(Lines);
	for (const FString& Ln : Lines)
	{
		TArray<FString> T; Ln.ParseIntoArrayWS(T); if (T.Num() < 3 || T[0].StartsWith(TEXT("#"))) { continue; }
		const int32 First = FCString::Atoi(*T[1]), Count = FCString::Atoi(*T[2]);
		if (Count > 0 && (First + Count) * 25 <= Data.Num()) { Clips.Add(FName(*T[0]), FIntPoint(First, Count)); }
	}
	return Clips.Num() > 0;
}

const float* FGkPoses::Frame(FName Clip, int32 Time) const
{
	const FIntPoint* C = Clips.Find(Clip); if (!C) { return nullptr; }
	return &Data[(C->X + FMath::Clamp(Time, 0, C->Y - 1)) * 25];
}

void FGkBody::Init(UPoseableMeshComponent* InMesh)
{
	Mesh = InMesh; Ref.Reset(); Alias.Reset(); if (!Mesh || !Mesh->GetSkinnedAsset()) { return; }
	const FReferenceSkeleton& RS = Mesh->GetSkinnedAsset()->GetRefSkeleton(); const TArray<FTransform>& Local = RS.GetRefBonePose();
	TArray<FTransform> CS; CS.SetNum(Local.Num());
	for (int32 b = 0; b < Local.Num(); b++) { const int32 Par = RS.GetParentIndex(b); CS[b] = Par >= 0 ? Local[b] * CS[Par] : Local[b]; Ref.Add(RS.GetBoneName(b), CS[b]); }
	if (RS.FindBoneIndex(TEXT("DEF-spine")) != INDEX_NONE)
	{
		static const TCHAR* Map[][2] = { { TEXT("pelvis"), TEXT("DEF-spine") }, { TEXT("spine_01"), TEXT("DEF-spine_001") }, { TEXT("spine_02"), TEXT("DEF-spine_002") }, { TEXT("spine_03"), TEXT("DEF-spine_003") },
			{ TEXT("neck_01"), TEXT("DEF-spine_004") }, { TEXT("neck_02"), TEXT("DEF-spine_005") }, { TEXT("head"), TEXT("DEF-spine_006") },
			{ TEXT("clavicle_l"), TEXT("DEF-shoulder_L") }, { TEXT("upperarm_l"), TEXT("DEF-upper_arm_L") }, { TEXT("lowerarm_l"), TEXT("DEF-forearm_L") }, { TEXT("hand_l"), TEXT("DEF-hand_L") },
			{ TEXT("clavicle_r"), TEXT("DEF-shoulder_R") }, { TEXT("upperarm_r"), TEXT("DEF-upper_arm_R") }, { TEXT("lowerarm_r"), TEXT("DEF-forearm_R") }, { TEXT("hand_r"), TEXT("DEF-hand_R") },
			{ TEXT("thigh_l"), TEXT("DEF-thigh_L") }, { TEXT("calf_l"), TEXT("DEF-shin_L") }, { TEXT("foot_l"), TEXT("DEF-foot_L") }, { TEXT("ball_l"), TEXT("DEF-toe_L") },
			{ TEXT("thigh_r"), TEXT("DEF-thigh_R") }, { TEXT("calf_r"), TEXT("DEF-shin_R") }, { TEXT("foot_r"), TEXT("DEF-foot_R") }, { TEXT("ball_r"), TEXT("DEF-toe_R") },
			{ TEXT("spine_04"), TEXT("-") }, { TEXT("spine_05"), TEXT("-") } };
		for (const auto& M : Map) { Alias.Add(FName(M[0]), FName(M[1])); }
	}
	// The solve expects a skeleton that stands facing +Y with its left hand toward +X. One that was brought in facing
	// another way is turned to that in its reference pose, so everything after reads it the same.
	const FTransform* HL = Ref.Find(Bone(TEXT("hand_l"))); const FTransform* HR = Ref.Find(Bone(TEXT("hand_r")));
	if (HL && HR)
	{
		FVector Side = HL->GetLocation() - HR->GetLocation(); Side.Z = 0.0;
		if (Side.Normalize())
		{
			const double Turn = FMath::Atan2(Side.Y, Side.X);   // how far her left-hand side is from +X
			if (FMath::Abs(Turn) > 0.05) { const FQuat Fix(FVector(0, 0, 1), -Turn); for (auto& It : Ref) { It.Value = FTransform(Fix * It.Value.GetRotation(), Fix.RotateVector(It.Value.GetLocation()), It.Value.GetScale3D()); } }
		}
	}
}

namespace
{
	// The browser's fighter, in its own units: thigh, shin, lower and upper spine, upper arm, forearm, half shoulder width, half hip width.
	const double TH = 23, SHN = 23, SP1 = 14, SP2 = 17, UA = 17, FA = 19, SW = 8.6, HW = 4.2;
	struct V3 { double x, y, z; };
	V3 operator+(const V3& a, const V3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	V3 operator-(const V3& a, const V3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	V3 operator*(const V3& a, double k) { return { a.x * k, a.y * k, a.z * k }; }
	double Len(const V3& a) { return FMath::Sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

	/** The browser's two-bone solve: from a root to a target with two lengths, bending toward the pole. Gives the middle joint and the end. */
	void Ik(const V3& r, const V3& t, double a, double b, const V3& pole, double maxS, V3& mid, V3& end)
	{
		double dx = t.x - r.x, dy = t.y - r.y, dz = t.z - r.z, d = FMath::Sqrt(dx * dx + dy * dy + dz * dz); if (d < 1e-3) { d = 1e-3; }
		const double L = (a + b) * 0.995; double sc = 1.0;
		if (d > L) { sc = FMath::Min(maxS, d / L); if (d > L * maxS) { const double q = L * maxS / d; dx *= q; dy *= q; dz *= q; d = L * maxS; } }
		const double Aa = a * sc, Bb = b * sc, ux = dx / d, uy = dy / d, uz = dz / d, pr = FMath::Clamp((Aa * Aa - Bb * Bb + d * d) / (2 * d), -Aa, Aa), h = FMath::Sqrt(FMath::Max(0.0, Aa * Aa - pr * pr));
		double px = pole.x, py = pole.y, pz = pole.z; const double dot = px * ux + py * uy + pz * uz; px -= dot * ux; py -= dot * uy; pz -= dot * uz;
		double pl = FMath::Sqrt(px * px + py * py + pz * pz); if (pl < 1e-9) { pl = 1.0; }
		mid = { r.x + ux * pr + px / pl * h, r.y + uy * pr + py / pl * h, r.z + uz * pr + pz / pl * h }; end = { r.x + dx, r.y + dy, r.z + dz };
	}

	FQuat Basis(const FVector& Dir, const FVector& Side) { return FRotationMatrix::MakeFromXZ(Dir.GetSafeNormal(), Side.GetSafeNormal()).ToQuat(); }
}

void FGkBody::Apply(const float* In, bool bAir, bool bMirror, float Roll, float Turn)
{
	if (!Mesh || !In || Ref.Num() == 0) { return; }
	const double D = PI / 180.0;
	// the 25 numbers: hip x y, lean, upper lean, chest twist, pelvis twist, head, elbow L R, knee L R, roll, turn, then hand L, hand R, foot L, foot R
	V3 hip = { In[0], In[1], 0 }; double ln = In[2], ln2 = In[3], tw = In[4], pt = In[5], hd = In[6], e[2] = { In[7], In[8] }, kk[2] = { In[9], In[10] }, rot = In[11] + Roll, yaw = In[12] + Turn;
	V3 hand[2] = { { In[13], In[14], In[15] }, { In[16], In[17], In[18] } }, foot[2] = { { In[19], In[20], In[21] }, { In[22], In[23], In[24] } };
	if (bMirror)
	{
		// the same pose seen in a mirror standing along his line of play: left and right change places
		Swap(hand[0], hand[1]); Swap(foot[0], foot[1]); Swap(e[0], e[1]); Swap(kk[0], kk[1]);
		for (int32 n = 0; n < 2; n++) { hand[n].z = -hand[n].z; foot[n].z = -foot[n].z; }
		tw = -tw; pt = -pt; yaw = -yaw;
	}
	// ---- the browser's solve, in its own space: x forward, y up, z toward his right
	const double a1 = ln * D, a2 = ln2 * D;
	const V3 cb = { hip.x + FMath::Sin(a1) * SP1, hip.y + FMath::Cos(a1) * SP1, 0 }, nk = { cb.x + FMath::Sin(a2) * SP2, cb.y + FMath::Cos(a2) * SP2, 0 };
	const V3 cax = { FMath::Sin(tw * D), 0, FMath::Cos(tw * D) }, pax = { FMath::Sin(pt * D), 0, FMath::Cos(pt * D) }; V3 mid = (cax + pax) * 0.5; if (Len(mid) < 0.05) { mid = pax; }
	const V3 sy = { nk.x - FMath::Sin(a2) * 3, nk.y - FMath::Cos(a2) * 3, 0 };
	V3 sh[2], el[2], wr[2], hs[2], kn[2], an[2], toe[2], armPole[2], legPole[2];
	for (int32 n = 0; n < 2; n++)
	{
		const double sd = n == 0 ? -1.0 : 1.0;
		sh[n] = { sy.x + cax.x * SW * sd, sy.y, cax.z * SW * sd };
		armPole[n] = { -0.35, e[n], sd * 0.7 * (e[n] > 0 ? 1.4 : 1.0) };
		Ik(sh[n], { hand[n].x, hand[n].y, sh[n].z * 0.45 + hand[n].z }, UA, FA, armPole[n], 1.75, el[n], wr[n]);
		hs[n] = { hip.x + pax.x * HW * sd, hip.y - 2, pax.z * HW * sd };
		const double fty = bAir ? foot[n].y : FMath::Max(4.0, double(foot[n].y));
		legPole[n] = { 0.8, 0.45, sd * (0.4 + kk[n] * 1.6) };
		Ik(hs[n], { foot[n].x, fty, sd * 5 + foot[n].z }, TH, SHN, legPole[n], 1.6, kn[n], an[n]);
		// the foot: flat on the floor, turning to follow the shin once it leaves the ground
		const double sx = an[n].x - kn[n].x, sy2 = an[n].y - kn[n].y, sl = FMath::Max(1e-6, FMath::Sqrt(sx * sx + sy2 * sy2)), k = bAir ? 1.0 : FMath::Clamp((an[n].y - 5) / 8, 0.0, 1.0);
		const double fx0 = sd > 0 && !bAir ? 0.8 : 1.0;
		toe[n] = { fx0 * (1 - k) + (-sy2 / sl * 0.85 + sx / sl * 0.35) * k, (sx / sl * 0.85 + sy2 / sl * 0.35) * k, (sd > 0 ? 0.6 : -0.15) * (1 - k) };
	}
	const double ha = a2 * 0.45 + hd * D, back = FMath::Clamp((FMath::Abs(tw) - 100) / 50, 0.0, 1.0);
	const V3 headSide = { -0.5 + back * 0.9 + cax.x * 0.12, 0, 0.82 };

	// ---- into this skeleton's space. The whole body is rolled and turned about the hips, then: forward is +Y, up is +Z, his right is -X.
	const double cz = FMath::Cos(-rot * D), sz = FMath::Sin(-rot * D), cy = FMath::Cos(yaw * D), sy3 = FMath::Sin(yaw * D);
	auto Dir = [&](const V3& v) { const double x1 = v.x * cz - v.y * sz, y1 = v.x * sz + v.y * cz, z1 = v.z; const double x2 = x1 * cy + z1 * sy3, z2 = -x1 * sy3 + z1 * cy; return FVector(-z2, x2, y1); };
	const double K = 100.0 / 51.0;
	auto Set = [this](const TCHAR* Name, const FQuat& Delta, const FVector* Where)
	{
		const FName B = Bone(Name); const FTransform* R = Ref.Find(B); if (!R) { return; }
		const FVector Loc = Where ? *Where : Mesh->GetBoneLocationByName(B, EBoneSpaces::ComponentSpace);
		Mesh->SetBoneTransformByName(B, FTransform(Delta * R->GetRotation(), Loc, R->GetScale3D()), EBoneSpaces::ComponentSpace);
	};
	auto RefDir = [this](const TCHAR* From, const TCHAR* To, const FVector& Fallback)
	{
		const FTransform* A = Ref.Find(Bone(From)); const FTransform* B = Ref.Find(Bone(To));
		const FVector V = (A && B) ? (B->GetLocation() - A->GetLocation()) : Fallback; return V.IsNearlyZero() ? Fallback : V.GetSafeNormal();
	};
	const FVector Up(0, 0, 1), Right(-1, 0, 0), Fwd(0, 1, 0); const FQuat Stand = Basis(Up, Right).Inverse();
	// hips and spine: each part points along the browser's spine and is squared to its hip or shoulder line
	// the hips ride at the height this skeleton's own legs reach (the browser's leg is 46 long and its ankle 4 off the floor)
	double LegK = 1.0;
	{ const FTransform* T = Ref.Find(Bone(TEXT("thigh_l"))); const FTransform* C = Ref.Find(Bone(TEXT("calf_l"))); const FTransform* F = Ref.Find(Bone(TEXT("foot_l")));
	  if (T && C && F) { LegK = FMath::Clamp((FVector::Dist(T->GetLocation(), C->GetLocation()) + FVector::Dist(C->GetLocation(), F->GetLocation()) + F->GetLocation().Z) / (50.0 * K), 0.7, 1.3); } }
	const FVector HipAt = FVector(0.0, hip.x * K, hip.y * K * LegK);
	Set(TEXT("pelvis"), Basis(Dir({ FMath::Sin(a1 * 0.5), FMath::Cos(a1 * 0.5), 0 }), Dir(pax)) * Stand, &HipAt);
	const FQuat Low = Basis(Dir(cb - hip), Dir(mid)) * Stand, High = Basis(Dir(nk - cb), Dir(cax)) * Stand, Head = Basis(Dir({ FMath::Sin(ha), FMath::Cos(ha), 0 }), Dir(headSide)) * Stand;
	Set(TEXT("spine_01"), Low, nullptr); Set(TEXT("spine_02"), Low, nullptr); Set(TEXT("spine_03"), High, nullptr); Set(TEXT("spine_04"), High, nullptr); Set(TEXT("spine_05"), High, nullptr);
	Set(TEXT("neck_01"), Head, nullptr); Set(TEXT("neck_02"), Head, nullptr); Set(TEXT("head"), Head, nullptr);
	// limbs: each bone points the way the browser's limb points, and bends about the same axis its joint bends about
	static const TCHAR* Arm[2][4] = { { TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l") }, { TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r") } };
	static const TCHAR* Leg[2][4] = { { TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l") }, { TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r") } };
	auto Hinge = [](const FVector& Upper, const FVector& Lower, const FVector& Pole) { FVector Ax = FVector::CrossProduct(Upper, Lower); if (Ax.SizeSquared() < 1e-4) { Ax = -FVector::CrossProduct(Upper, Pole); } return Ax.GetSafeNormal(); };
	for (int32 n = 0; n < 2; n++)
	{
		const int32 s = n;   // 0 is his left, 1 his right (a mirrored pose has already had its sides exchanged)
		{
			const FVector RU = RefDir(Arm[s][1], Arm[s][2], -Up), RL = RefDir(Arm[s][2], Arm[s][3], -Up), RAx = Hinge(RU, RL, -Fwd);
			const FVector NU = Dir(el[n] - sh[n]).GetSafeNormal(), NL = Dir(wr[n] - el[n]).GetSafeNormal(), NAx = Hinge(NU, NL, Dir(armPole[n]));
			const FQuat QU = Basis(NU, NAx) * Basis(RU, RAx).Inverse(), QL = Basis(NL, NAx) * Basis(RL, RAx).Inverse();
			Set(Arm[s][1], QU, nullptr); Set(Arm[s][2], QL, nullptr); Set(Arm[s][3], QL, nullptr);
		}
		{
			const FVector RU = RefDir(Leg[s][0], Leg[s][1], -Up), RL = RefDir(Leg[s][1], Leg[s][2], -Up), RAx = Hinge(RU, RL, Fwd);
			const FVector NU = Dir(kn[n] - hs[n]).GetSafeNormal(), NL = Dir(an[n] - kn[n]).GetSafeNormal(), NAx = Hinge(NU, NL, Dir(legPole[n]));
			Set(Leg[s][0], Basis(NU, NAx) * Basis(RU, RAx).Inverse(), nullptr); Set(Leg[s][1], Basis(NL, NAx) * Basis(RL, RAx).Inverse(), nullptr);
			const FVector RF = RefDir(Leg[s][2], Leg[s][3], Fwd);
			Set(Leg[s][2], Basis(Dir(toe[n]), Dir(pax)) * Basis(RF, Right).Inverse(), nullptr);
		}
	}
}
