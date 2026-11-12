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

/** How one move is shown: the poses it passes through, the limb steered onto its hitbox, and its swing trail. */
struct FGkMoveAnim
{
	FName Wind, Strike, Rec;
	int32 Limb = 0;                 // 0 none, 1 left hand, 2 right hand, 3 left foot, 4 right foot
	float Arc = 0.f, ArcSize = 0.f; // trail angle on screen for a fighter facing right, and its size in centimetres
	int32 ArcKind = 4;
	float Spin = 0.f;
};

/** Every pose and move animation, read from Content/Gk/kagu.txt so they can be reworked without rebuilding the game. */
struct FGkLibrary
{
	TMap<FName, FGkPose> Poses; TMap<FName, FGkMoveAnim> Moves;
	bool Load(const FString& Path, FString& OutError);
	const FGkPose& Pose(FName Name) const;
	const FGkMoveAnim& Move(FName Name) const;
};
FGkLibrary& GkLib();
FGkPose GkBlend(const FGkPose& A, const FGkPose& B, float K);
FGkPose GkMirror(const FGkPose& P);

/**
 * Baked animation. Every frame of every clip is a key pose made by tools/anim; nothing is interpolated here.
 * A frame holds, for each bone, the transform that carries the bind pose to the key pose, as authored and mirrored.
 */
struct FGkClip { TArray<int32> Idx; bool bLoop = false; };
struct FGkAnim
{
	int32 NumFrames = 0; TArray<FName> Bones; TArray<float> Data; TMap<FName, FGkClip> Clips;
	bool Load(const FString& Dir, FString& OutError);
	/** The baked frame a clip shows at a given time, or -1 if there is no such clip. */
	int32 Frame(FName Clip, int32 Time) const;
	FTransform Delta(int32 FrameIndex, bool bMirror, int32 Bone) const;
};
FGkAnim& GkAnim();

/** Turns a pose into component-space bone transforms for the character's skeleton. */
struct FGkRig
{
	TArray<FName> Names; TArray<int32> Parent; TArray<FTransform> RefLocal, RefCS; TMap<FName, int32> Index;
	void Init(const FReferenceSkeleton& Skel);
	void Solve(const FGkPose& P, TArray<FTransform>& Out) const;
	bool IsValid() const { return Names.Num() > 0 && Index.Contains(TEXT("Hips")); }
};
