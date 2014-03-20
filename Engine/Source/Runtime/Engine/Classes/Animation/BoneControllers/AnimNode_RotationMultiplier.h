// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_RotationMultiplier.generated.h"

UENUM()
enum EBoneAxis
{
	BA_X,
	BA_Y,
	BA_Z
};

/**
 *	Simple controller that multiplies scalar value to the translation/rotation/scale of a single bone.
 */

USTRUCT()
struct ENGINE_API FAnimNode_RotationMultiplier : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Multiplier) 
	FBoneReference	TargetBone;

	/** Source to get transform from **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Multiplier)
	FBoneReference	SourceBone;

	// To make these to be easily pin-hookable, I'm not making it struct, but each variable
	// 0.f is invalid, and default
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Multiplier, meta=(PinShownByDefault))
	float	Multiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Multiplier)
	TEnumAsByte<EBoneAxis> RotationAxisToRefer;

	FAnimNode_RotationMultiplier();

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, const FBoneContainer & RequiredBones, FA2CSPose& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) OVERRIDE;
	virtual bool IsValidToEvaluate(const USkeleton * Skeleton, const FBoneContainer & RequiredBones) OVERRIDE;
	// End of FAnimNode_SkeletalControlBase interface

private:
	// Extract Delta Quat of rotation around Axis of animation and reference pose for the SourceBoneIndex
	FQuat ExtractAngle(const TArray<FTransform> & RefPoseTransforms, FA2CSPose& MeshBases, const EBoneAxis Axis,  int32 SourceBoneIndex);
	// Multiply scalar value Multiplier to the delta Quat of SourceBone Index's rotation
	void MultiplyQuatBasedOnSourceIndex(const TArray<FTransform> & RefPoseTransforms, FA2CSPose& MeshBases, const EBoneAxis Axis, int32 SourceBoneIndex, float Multiplier, const FQuat & ReferenceQuat, FQuat & OutQuat);

	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer & RequiredBones) OVERRIDE;
	// End of FAnimNode_SkeletalControlBase interface
};
