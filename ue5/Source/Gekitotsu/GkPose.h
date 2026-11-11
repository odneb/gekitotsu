#pragma once
#include "CoreMinimal.h"

/**
 * A pose for the fighter, written as targets in his own space: x = his left, y = the way he faces, z = up, in
 * centimetres from the point between his feet. Limbs are solved with two-bone IK. Every pose in the game is
 * authored this way for this character; none come from an animation library.
 */
struct FGkPose
{
	FVector Hip = FVector(0, -7, 83);
	float Ln = 0, Ln2 = 0, Sd = 0, Tw = 0, Pt = 0, Hd = 0, Hy = 0;   // spine lean (lower, upper), side bend, chest twist, pelvis twist, head pitch, head turn
	FVector HL = FVector(13, 46, 132), HR = FVector(-25, -18, 80), FL = FVector(9, 26, 9), FR = FVector(-13, -27, 9);
	FVector EL = FVector(0.3, -0.2, -1), ER = FVector(-0.8, -0.6, 0.2), KL = FVector(0.5, 1, 0), KR = FVector(-0.9, 0.6, 0), TL = FVector(0.1, 1, -0.5), TR = FVector(-0.8, 0.5, -0.5);
	float Roll = 0, Spin = 0;   // whole-body tumble about the side axis, and turn about the vertical
};

enum class EGkPose : uint8
{
	Idle, Idle2, Crouch, WalkA, WalkB, Jump, Fall, Dash, BackDash, BlockS, BlockC, Hit, Launch, Down, Rise, Parry,
	LS, MW, MS, HW, HS, CL, CM, CHW, CHS, JL, JM, JH, JD, FireW, FireS, SurgeW, SurgeS, RiseS, Vanish, Whirl, Dive, HomeW, HomeS, Grab, Held, SuperS, Win, Count
};

const FGkPose& GkPose(EGkPose P);
FGkPose GkBlend(const FGkPose& A, const FGkPose& B, float K);

/** Turns a pose into component-space bone transforms for the character's skeleton. */
struct FGkRig
{
	TArray<FName> Names; TArray<int32> Parent; TArray<FTransform> RefLocal, RefCS; TMap<FName, int32> Index;
	void Init(const FReferenceSkeleton& Skel);
	void Solve(const FGkPose& P, TArray<FTransform>& Out) const;
	bool IsValid() const { return Names.Num() > 0 && Index.Contains(TEXT("Hips")); }
};
