// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*
* Copyright 2010 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

/*=============================================================================
  Implementation of animation export related functionality from FbxExporter
=============================================================================*/

#include "UnrealEd.h"

DEFINE_LOG_CATEGORY_STATIC(LogFbxAnimationExport, Log, All);

#include "FbxExporter.h"

namespace UnFbx
{


void FFbxExporter::ExportAnimSequenceToFbx(const UAnimSequence* AnimSeq,
									 const USkeletalMesh* SkelMesh,
									 TArray<FbxNode*>& BoneNodes,
									 FbxAnimLayer* InAnimLayer,
									 float AnimStartOffset,
									 float AnimEndOffset,
									 float AnimPlayRate,
									 float StartTime,
									 bool bLooping)
{
	USkeleton * Skeleton = AnimSeq->GetSkeleton();

	// Add the animation data to the bone nodes
	for(int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex)
	{
		FbxNode* CurrentBoneNode = BoneNodes[BoneIndex];

		// Create the AnimCurves
		FbxAnimCurve* Curves[6];
		Curves[0] = CurrentBoneNode->LclTranslation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[1] = CurrentBoneNode->LclTranslation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[2] = CurrentBoneNode->LclTranslation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		Curves[3] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[4] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[5] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		float AnimTime					= AnimStartOffset;
		float AnimEndTime				= (AnimSeq->SequenceLength - AnimEndOffset);
		const float AnimTimeIncrement	= (AnimSeq->SequenceLength / AnimSeq->NumFrames) * AnimPlayRate;

		FbxTime ExportTime;
		ExportTime.SetSecondDouble(StartTime);

		FbxTime ExportTimeIncrement;
		ExportTimeIncrement.SetSecondDouble( (AnimSeq->SequenceLength / AnimSeq->NumFrames) );

		int32 BoneTreeIndex = Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex(SkelMesh, BoneIndex);
		int32 BoneTrackIndex = Skeleton->GetAnimationTrackIndex(BoneTreeIndex, AnimSeq);
		if(BoneTrackIndex == INDEX_NONE)
		{
			// If this sequence does not have a track for the current bone, then skip it
			continue;
		}

		for(int32 i = 0; i < 6; ++i)
		{
			Curves[i]->KeyModifyBegin();
		}

		bool bLastKey = false;
		// Step through each frame and add the bone's transformation data
		while (AnimTime < AnimEndTime)
		{
			FTransform BoneAtom;
			AnimSeq->GetBoneTransform(BoneAtom, BoneTrackIndex, AnimTime, bLooping, true);

			FbxVector4 Translation = Converter.ConvertToFbxPos(BoneAtom.GetTranslation());

			FbxVector4 Rotation = Converter.ConvertToFbxRot(BoneAtom.GetRotation().Euler());
		
			int32 lKeyIndex;

			AnimTime += AnimTimeIncrement;

			bLastKey = AnimTime >= AnimEndTime;
			for(int32 i = 0, j=3; i < 3; ++i, ++j)
			{
				lKeyIndex = Curves[i]->KeyAdd(ExportTime);
				Curves[i]->KeySetValue(lKeyIndex, Translation[i]);
				Curves[i]->KeySetInterpolation(lKeyIndex, bLastKey ? FbxAnimCurveDef::eInterpolationConstant : FbxAnimCurveDef::eInterpolationCubic);

				if( bLastKey )
				{
					Curves[i]->KeySetConstantMode( lKeyIndex, FbxAnimCurveDef::eConstantStandard );
				}

				lKeyIndex = Curves[j]->KeyAdd(ExportTime);
				Curves[j]->KeySetValue(lKeyIndex, Rotation[i]);
				Curves[j]->KeySetInterpolation(lKeyIndex, bLastKey ? FbxAnimCurveDef::eInterpolationConstant : FbxAnimCurveDef::eInterpolationCubic);

				if( bLastKey )
				{
					Curves[j]->KeySetConstantMode( lKeyIndex, FbxAnimCurveDef::eConstantStandard );
				}
			}

			ExportTime += ExportTimeIncrement;
		
		
		}

		for(int32 i = 0; i < 6; ++i)
		{
			Curves[i]->KeyModifyEnd();
		}
	}
}


// The curve code doesn't differentiate between angles and other data, so an interpolation from 179 to -179
// will cause the bone to rotate all the way around through 0 degrees.  So here we make a second pass over the 
// rotation tracks to convert the angles into a more interpolation-friendly format.  
void FFbxExporter::CorrectAnimTrackInterpolation( TArray<FbxNode*>& BoneNodes, FbxAnimLayer* InAnimLayer )
{
	// Add the animation data to the bone nodes
	for(int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex)
	{
		FbxNode* CurrentBoneNode = BoneNodes[BoneIndex];

		// Fetch the AnimCurves
		FbxAnimCurve* Curves[3];
		Curves[0] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[1] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[2] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		for(int32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex)
		{
			FbxAnimCurve* CurrentCurve = Curves[CurveIndex];

			float CurrentAngleOffset = 0.f;
			for(int32 KeyIndex = 1; KeyIndex < CurrentCurve->KeyGetCount(); ++KeyIndex)
			{
				float PreviousOutVal	= CurrentCurve->KeyGetValue( KeyIndex-1 );
				float CurrentOutVal		= CurrentCurve->KeyGetValue( KeyIndex );

				float DeltaAngle = (CurrentOutVal + CurrentAngleOffset) - PreviousOutVal;

				if(DeltaAngle >= 180)
				{
					CurrentAngleOffset -= 360;
				}
				else if(DeltaAngle <= -180)
				{
					CurrentAngleOffset += 360;
				}

				CurrentOutVal += CurrentAngleOffset;

				CurrentCurve->KeySetValue(KeyIndex, CurrentOutVal);
			}
		}
	}
}


void FFbxExporter::ExportAnimSequence( const UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, bool bExportSkelMesh )
{
	if( Scene == NULL || AnimSeq == NULL || SkelMesh == NULL )
	{
 		return;
	}

	FbxString NodeName("BaseNode");

	FbxNode* BaseNode = FbxNode::Create(Scene, NodeName);
	Scene->GetRootNode()->AddChild(BaseNode);

	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton(*SkelMesh, BoneNodes);
	BaseNode->AddChild(SkeletonRootNode);


	// Export the anim sequence
	{
		ExportAnimSequenceToFbx(AnimSeq,
			SkelMesh,
			BoneNodes,
			AnimLayer,
			0.f,		// AnimStartOffset
			0.f,		// AnimEndOffset
			1.f,		// AnimPlayRate
			0.f,		// StartTime
			false);		// bLooping

		CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);
	}


	// Optionally export the mesh
	if(bExportSkelMesh)
	{
		FString MeshName;
		SkelMesh->GetName(MeshName);

		// Add the mesh
		FbxNode* MeshRootNode = CreateMesh(*SkelMesh, *MeshName);
		if(MeshRootNode)
		{
			BaseNode->AddChild(MeshRootNode);
		}

		if(SkeletonRootNode && MeshRootNode)
		{
			// Bind the mesh to the skeleton
			BindMeshToSkeleton(*SkelMesh, MeshRootNode, BoneNodes);

			// Add the bind pose
			CreateBindPose(MeshRootNode);
		}
	}
}


void FFbxExporter::ExportAnimSequencesAsSingle( USkeletalMesh* SkelMesh, const ASkeletalMeshActor* SkelMeshActor, const FString& ExportName, const TArray<UAnimSequence*>& AnimSeqList, const TArray<struct FAnimControlTrackKey>& TrackKeys )
{
	if (Scene == NULL || SkelMesh == NULL || AnimSeqList.Num() == 0 || AnimSeqList.Num() != TrackKeys.Num()) return;

	FbxNode* BaseNode = FbxNode::Create(Scene, Converter.ConvertToFbxString(ExportName));
	Scene->GetRootNode()->AddChild(BaseNode);

	if( SkelMeshActor )
	{
		// Set the default position of the actor on the transforms
		// The Unreal transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		BaseNode->LclTranslation.Set(Converter.ConvertToFbxPos(SkelMeshActor->GetActorLocation()));
		BaseNode->LclRotation.Set(Converter.ConvertToFbxRot(SkelMeshActor->GetActorRotation().Euler()));
		BaseNode->LclScaling.Set(Converter.ConvertToFbxScale(SkelMeshActor->GetRootComponent()->RelativeScale3D));

	}

	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton(*SkelMesh, BoneNodes);
	BaseNode->AddChild(SkeletonRootNode);

	bool bAnyObjectMissingSourceData = false;
	float ExportStartTime = 0.f;
	for(int32 AnimSeqIndex = 0; AnimSeqIndex < AnimSeqList.Num(); ++AnimSeqIndex)
	{
		const UAnimSequence* AnimSeq = AnimSeqList[AnimSeqIndex];
		const FAnimControlTrackKey& TrackKey = TrackKeys[AnimSeqIndex];

		// Shift the anim sequences so the first one is at time zero in the FBX file
		const float CurrentStartTime = TrackKey.StartTime - ExportStartTime;

		ExportAnimSequenceToFbx(AnimSeq,
			SkelMesh,
			BoneNodes,
			AnimLayer,
			TrackKey.AnimStartOffset,
			TrackKey.AnimEndOffset,
			TrackKey.AnimPlayRate,
			CurrentStartTime,
			TrackKey.bLooping);
	}

	CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);

	if (bAnyObjectMissingSourceData)
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Exporter_Error_SourceDataUnavailable", "No source data available for some objects.  See the log for details.") );
	}

}

/**
 * Exports all the animation sequences part of a single Group in a Matinee sequence
 * as a single animation in the FBX document.  The animation is created by sampling the
 * sequence at DEFAULT_SAMPLERATE updates/second and extracting the resulting bone transforms from the given
 * skeletal mesh
 */
void FFbxExporter::ExportMatineeGroup(class AMatineeActor* MatineeActor, USkeletalMeshComponent* SkeletalMeshComponent)
{
	static const float SamplingRate = 1.f / DEFAULT_SAMPLERATE;

	float MatineeLength = MatineeActor->MatineeData->InterpLength;

	
	if (Scene == NULL || MatineeActor == NULL || SkeletalMeshComponent == NULL || MatineeLength == 0)
	{
		return;
	}

	FbxString NodeName("MatineeSequence");

	FbxNode* BaseNode = FbxNode::Create(Scene, NodeName);
	Scene->GetRootNode()->AddChild(BaseNode);
	AActor* Owner = SkeletalMeshComponent->GetOwner();
	if( Owner && Owner->GetRootComponent() )
	{
		// Set the default position of the actor on the transforms
		// The UE3 transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		BaseNode->LclTranslation.Set(Converter.ConvertToFbxPos( Owner->GetActorLocation() ) );
		BaseNode->LclRotation.Set(Converter.ConvertToFbxRot( Owner->GetActorRotation().Euler() ) ) ;
		BaseNode->LclScaling.Set(Converter.ConvertToFbxScale( Owner->GetRootComponent()->RelativeScale3D ) );
	}
	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton(*SkeletalMeshComponent->SkeletalMesh, BoneNodes);
	BaseNode->AddChild(SkeletonRootNode);

	// show a status update every 1 second worth of samples
	const float UpdateFrequency = 1.0f;
	float NextUpdateTime = UpdateFrequency;

	float SampleTime;
	for(SampleTime = 0.f; SampleTime <= MatineeLength; SampleTime += SamplingRate)
	{
		// This will call UpdateSkelPose on the skeletal mesh component to move bones based on animations in the matinee group
		MatineeActor->UpdateInterp( SampleTime, true );

		FbxTime ExportTime;
		ExportTime.SetSecondDouble(SampleTime);

		NextUpdateTime -= SamplingRate;

		if( NextUpdateTime <= 0.0f )
		{
			NextUpdateTime = UpdateFrequency;
			GWarn->StatusUpdate( FMath::Round( SampleTime ), FMath::Round(MatineeLength), NSLOCTEXT("FbxExporter", "ExportingToFbxStatus", "Exporting to FBX") );
		}

		// Add the animation data to the bone nodes
		for(int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex)
		{
			FName BoneName = SkeletalMeshComponent->SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex);
			FbxNode* CurrentBoneNode = BoneNodes[BoneIndex];

			// Create the AnimCurves
			FbxAnimCurve* Curves[6];
			Curves[0] = CurrentBoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			Curves[1] = CurrentBoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			Curves[2] = CurrentBoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

			Curves[3] = CurrentBoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			Curves[4] = CurrentBoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			Curves[5] = CurrentBoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

			for(int32 i = 0; i < 6; ++i)
			{
				Curves[i]->KeyModifyBegin();
			}

			FTransform BoneTransform = SkeletalMeshComponent->LocalAtoms[BoneIndex];
			FbxVector4 Translation = Converter.ConvertToFbxPos(BoneTransform.GetLocation());
			FbxVector4 Rotation = Converter.ConvertToFbxRot(BoneTransform.GetRotation().Euler());

			int32 lKeyIndex;

			for(int32 i = 0, j=3; i < 3; ++i, ++j)
			{
				lKeyIndex = Curves[i]->KeyAdd(ExportTime);
				Curves[i]->KeySetValue(lKeyIndex, Translation[i]);
				Curves[i]->KeySetInterpolation(lKeyIndex, FbxAnimCurveDef::eInterpolationCubic);

				lKeyIndex = Curves[j]->KeyAdd(ExportTime);
				Curves[j]->KeySetValue(lKeyIndex, Rotation[i]);
				Curves[j]->KeySetInterpolation(lKeyIndex, FbxAnimCurveDef::eInterpolationCubic);
			}

			for(int32 i = 0; i < 6; ++i)
			{
				Curves[i]->KeyModifyEnd();
			}
		}
	}

	CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);
}

} // namespace UnFbx
