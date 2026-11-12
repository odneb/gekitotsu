#pragma once
#include "CoreMinimal.h"

class UPoseableMeshComponent;

/**
 * The browser game's animation, as data: for every move and every state, the pose it shows on each frame, sampled
 * from the browser's own pose code by tools/bake_poses.js. A pose is 25 numbers: where the hips are, how the spine
 * leans and twists, and where each hand and foot is.
 */
struct FGkPoses
{
	TMap<FName, FIntPoint> Clips; TArray<float> Data;
	bool Load(const FString& Dir);
	/** One frame of a clip (the last one if Time runs past its end), or null if there is no such clip. */
	const float* Frame(FName Clip, int32 Time) const;
	int32 Length(FName Clip) const { const FIntPoint* C = Clips.Find(Clip); return C ? C->Y : 0; }
};

/** One fighter's skeleton. Given a pose, it solves the limbs the way the browser does and sets the bones to match. */
struct FGkBody
{
	UPoseableMeshComponent* Mesh = nullptr; TMap<FName, FTransform> Ref;
	void Init(UPoseableMeshComponent* InMesh);
	/** Roll and Turn are added to the pose's own whole-body rotation, in degrees. A mirrored pose is the same pose for a fighter facing left. */
	void Apply(const float* Pose, bool bAir, bool bMirror, float Roll, float Turn);
};
