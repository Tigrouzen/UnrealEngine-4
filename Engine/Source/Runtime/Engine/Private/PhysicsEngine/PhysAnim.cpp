// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysAnim.cpp: Code for supporting animation/physics blending
=============================================================================*/ 

#include "EnginePrivate.h"

#if WITH_PHYSX
	#include "PhysXSupport.h"
#endif // WITH_PHYSX


/** Used for drawing pre-phys skeleton if bShowPrePhysBones is true */
static const FColor AnimSkelDrawColor(255, 64, 64);

// Temporary workspace for caching world-space matrices.
struct FAssetWorldBoneTM
{
	FTransform	TM; // Should never contain scaling.
	int32			UpdateNum; // If this equals PhysAssetUpdateNum, then the matrix is up to date.
};

// @todo anim: this has to change to make multi-threading working. 
static int32 PhysAssetUpdateNum = 0;
static TArray<FAssetWorldBoneTM> WorldBoneTMs;

// Use current pose to calculate world-space position of this bone without physics now.
static void UpdateWorldBoneTM(int32 BoneIndex, USkeletalMeshComponent* SkelComp, const FVector & Scale3D)
{
	// If its already up to date - do nothing
	if(	WorldBoneTMs[BoneIndex].UpdateNum == PhysAssetUpdateNum )
	{
		return;
	}

	FTransform ParentTM, RelTM;
	if(BoneIndex == 0)
	{
		// If this is the root bone, we use the mesh component LocalToWorld as the parent transform.
		FTransform LocalToWorldTM = SkelComp->ComponentToWorld;
		LocalToWorldTM.RemoveScaling();
		ParentTM = LocalToWorldTM;
	}
	else
	{
		// If not root, use our cached world-space bone transforms.
		int32 ParentIndex = SkelComp->SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
		UpdateWorldBoneTM(ParentIndex, SkelComp, Scale3D);
		ParentTM = WorldBoneTMs[ParentIndex].TM;
	}

	RelTM = SkelComp->LocalAtoms[BoneIndex];
	RelTM.ScaleTranslation( Scale3D );

	WorldBoneTMs[BoneIndex].TM = RelTM * ParentTM;
	WorldBoneTMs[BoneIndex].UpdateNum = PhysAssetUpdateNum;
}


void USkeletalMeshComponent::BlendPhysicsBones( TArray<FBoneIndexType>& InRequiredBones)
{
	// Get drawscale from Owner (if there is one)
	FVector TotalScale3D = ComponentToWorld.GetScale3D();

	FVector RecipScale3D = TotalScale3D.Reciprocal();

	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	check( PhysicsAsset );

	// Make sure scratch space is big enough.
	WorldBoneTMs.Reset();
	WorldBoneTMs.AddUninitialized(SpaceBases.Num());
	PhysAssetUpdateNum++;

	FTransform LocalToWorldTM = ComponentToWorld;
	LocalToWorldTM.RemoveScaling();

	// For each bone - see if we need to provide some data for it.
	for(int32 i=0; i<InRequiredBones.Num(); i++)
	{
		int32 BoneIndex = InRequiredBones[i];

		// See if this is a physics bone..
		int32 BodyIndex = PhysicsAsset->FindBodyIndex( SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex));
		// need to update back to physX so that physX knows where it was after blending
		bool bUpdatePhysics = false;
		FBodyInstance* BodyInstance = NULL;
		// if this is true fixed bones haven't been updated, so we can't blend between. 
		bool bShouldSkipFixedBones = KinematicBonesUpdateType == EKinematicBonesUpdateToPhysics::SkipFixedAndSimulatingBones;
		// If so - get its world space matrix and its parents world space matrix and calc relative atom.
		if(BodyIndex != INDEX_NONE )
		{	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// tracking down TTP 280421. Remove this if this doesn't happen. 
			if ( !ensure(Bodies.IsValidIndex(BodyIndex)) )
			{
				UE_LOG(LogPhysics, Warning, TEXT("%s(Mesh %s, PhysicsAsset %s)"), 
					*GetName(), *GetNameSafe(SkeletalMesh), *GetNameSafe(PhysicsAsset));
				if ( PhysicsAsset )
				{
					UE_LOG(LogPhysics, Warning, TEXT(" - # of BodySetup (%d), # of Bodies (%d), Invalid BodyIndex(%d)"), 
						PhysicsAsset->BodySetup.Num(), Bodies.Num(), BodyIndex);
				}
				continue;
			}
#endif
			BodyInstance = Bodies[BodyIndex];
			UBodySetup* BodySetup = PhysicsAsset->BodySetup[BodyIndex];
			// since we don't copy back to physics, we shouldn't blend fixed bones here if this set up is used
			// if you'd like to use fixed bones to blend use SkipSimulatingBones
			// you can simulate fixed bones by setting it to instance, but that case, please use SkipSimulating Bones. 
			bool bSkipFixedBones =  bShouldSkipFixedBones
				&& (BodySetup? BodySetup->PhysicsType == PhysType_Fixed : false);

			if(!bSkipFixedBones && BodyInstance->IsValidBodyInstance())
			{
				FTransform PhysTM = BodyInstance->GetUnrealWorldTransform();

				// Store this world-space transform in cache.
				WorldBoneTMs[BoneIndex].TM = PhysTM;
				WorldBoneTMs[BoneIndex].UpdateNum = PhysAssetUpdateNum;

				float UsePhysWeight = (bBlendPhysics)? 1.f : BodyInstance->PhysicsBlendWeight;

				// Find this bones parent matrix.
				FTransform ParentWorldTM;

				// if we wan't 'full weight' we just find 
				if(UsePhysWeight > 0.f)
				{
					if(BoneIndex == 0)
					{
						ParentWorldTM = LocalToWorldTM;
					}
					else
					{
						// If not root, get parent TM from cache (making sure its up-to-date).
						int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
						UpdateWorldBoneTM(ParentIndex, this, TotalScale3D);
						ParentWorldTM = WorldBoneTMs[ParentIndex].TM;
					}


					// Then calc rel TM and convert to atom.
					FTransform RelTM = PhysTM.GetRelativeTransform(ParentWorldTM);
					RelTM.RemoveScaling();
					FQuat RelRot(RelTM.GetRotation());
					FVector RelPos =  RecipScale3D * RelTM.GetLocation();
					FTransform PhysAtom = FTransform(RelRot, RelPos, LocalAtoms[BoneIndex].GetScale3D());

					// Now blend in this atom. See if we are forcing this bone to always be blended in
					LocalAtoms[BoneIndex].Blend( LocalAtoms[BoneIndex], PhysAtom, UsePhysWeight );

					if (UsePhysWeight < 1.f)
					{
						bUpdatePhysics = true;
					}
				}
				else
				{
					WorldBoneTMs[BoneIndex].UpdateNum = 0;
				}
			}
			else
			{
				WorldBoneTMs[BoneIndex].UpdateNum = 0;
			}
		}
		else
		{
			WorldBoneTMs[BoneIndex].UpdateNum = 0;
		}

		// Update SpaceBases entry for this bone now
		if( BoneIndex == 0 )
		{
			SpaceBases[0] = LocalAtoms[0];
		}
		else
		{
			const int32 ParentIndex	= SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
			SpaceBases[BoneIndex]	= LocalAtoms[BoneIndex] * SpaceBases[ParentIndex];
		}

		if (bUpdatePhysics && BodyInstance)
		{
			BodyInstance->SetBodyTransform(SpaceBases[BoneIndex] * ComponentToWorld, true);
		}
	}

	// Transforms updated, cached local bounds are now out of date.
	InvalidateCachedBounds();
}



bool USkeletalMeshComponent::ShouldBlendPhysicsBones()
{
	if (bUseSingleBodyPhysics)
	{
		return false;
	}

	for (int32 BodyIndex = 0; BodyIndex < Bodies.Num(); ++BodyIndex)
	{
		if (Bodies[BodyIndex]->PhysicsBlendWeight > 0.f)
		{
			return true;
		}
	}

	return false;
}

void USkeletalMeshComponent::BlendInPhysics()
{
	SCOPE_CYCLE_COUNTER(STAT_BlendInPhysics);

	// Can't do anything without a SkeletalMesh
	if( !SkeletalMesh )
	{
		return;
	}

	// We now have all the animations blended together and final relative transforms for each bone.
	// If we don't have or want any physics, we do nothing.
	if( Bodies.Num() > 0 && (bBlendPhysics || ShouldBlendPhysicsBones()) )
	{
		BlendPhysicsBones( RequiredBones );
		
		// Update Child Transform - The above function changes bone transform, so will need to update child transform
		UpdateChildTransforms();

		// animation often change overlap. 
		UpdateOverlaps();

		// New bone positions need to be sent to render thread
		MarkRenderDynamicDataDirty();
	}
}


void USkeletalMeshComponent::UpdateKinematicBonesToPhysics(bool bTeleport)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateRBBones);

	// This below code produces some interesting result here
	// - below codes update physics data, so if you don't update pose, the physics won't have the right result
	// - but if we just update physics bone without update current pose, it will have stale data
	// If desired, pass the animation data to the physics joints so they can be used by motors.
	// See if we are going to need to update kinematics
	const bool bUpdateKinematics = (!bUseSingleBodyPhysics && KinematicBonesUpdateType != EKinematicBonesUpdateToPhysics::SkipAllBones);

	// If desired, update physics bodies associated with skeletal mesh component to match.
	if( !bUpdateKinematics )
	{
		// nothing to do 
		return;
	}

	const FTransform& CurrentLocalToWorld = ComponentToWorld;

	if(CurrentLocalToWorld.ContainsNaN())
	{
		return;
	}

	// If desired, draw the skeleton at the point where we pass it to the physics.
	if( bShowPrePhysBones && SkeletalMesh && SpaceBases.Num() == SkeletalMesh->RefSkeleton.GetNum() )
	{
		for(int32 i=1; i<SpaceBases.Num(); i++)
		{
			FVector ThisPos = CurrentLocalToWorld.TransformPosition( SpaceBases[i].GetLocation() );

			int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(i);
			FVector ParentPos = CurrentLocalToWorld.TransformPosition( SpaceBases[ParentIndex].GetLocation() );

			GetWorld()->LineBatcher->DrawLine(ThisPos, ParentPos, AnimSkelDrawColor, SDPG_Foreground);
		}
	}

	// warn if it has non-uniform scale
	const FVector & MeshScale3D = CurrentLocalToWorld.GetScale3D();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if( !MeshScale3D.IsUniform() )
	{
		UE_LOG(LogPhysics, Log, TEXT("USkeletalMeshComponent::UpdateKinematicBonesToPhysics : Non-uniform scale factor (%s) can cause physics to mismatch for %s  SkelMesh: %s"), *MeshScale3D.ToString(), *GetFullName(), SkeletalMesh ? *SkeletalMesh->GetFullName() : TEXT("NULL"));
	}
#endif
	const UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && SkeletalMesh && Bodies.Num() > 0)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if ( !ensure( PhysicsAsset->BodySetup.Num() == Bodies.Num()) )
		{
			// related to TTP 280315
			UE_LOG(LogPhysics, Warning, TEXT("Mesh (%s) has PhysicsAsset(%s), and BodySetup(%d) and Bodies(%d) don't match"), 
				*SkeletalMesh->GetName(), *PhysicsAsset->GetName(), PhysicsAsset->BodySetup.Num(), Bodies.Num());
			return;
		}
#endif

		// see if we should check fixed bones or not
		bool bShouldSkipFixedBones = KinematicBonesUpdateType == EKinematicBonesUpdateToPhysics::SkipFixedAndSimulatingBones;

		// Iterate over each body
		for(int32 i = 0; i < Bodies.Num(); i++)
		{
			// If we have a physics body, and its kinematic...
			FBodyInstance* BodyInst = Bodies[i];
			check(BodyInst);
			// special flag to check to see if we should update fixed bones or not
			bool bSkipFixedBones =  bShouldSkipFixedBones
				&& (BodyInst->BodySetup.IsValid()? BodyInst->BodySetup.Get()->PhysicsType == PhysType_Fixed : false);

			if(!bSkipFixedBones && BodyInst->IsValidBodyInstance() && !BodyInst->IsInstanceSimulatingPhysics())
			{
				// Find the graphics bone index that corresponds to this physics body.
				FName const BodyName = PhysicsAsset->BodySetup[i]->BoneName;
				int32 const BoneIndex = SkeletalMesh->RefSkeleton.FindBoneIndex(BodyName);

				// If we could not find it - warn.
				if( BoneIndex == INDEX_NONE || BoneIndex >= SpaceBases.Num() )
				{
					UE_LOG(LogPhysics, Log,  TEXT("UpdateRBBones: WARNING: Failed to find bone '%s' need by PhysicsAsset '%s' in SkeletalMesh '%s'."), *BodyName.ToString(), *PhysicsAsset->GetName(), *SkeletalMesh->GetName() );
				}
				else
				{
					// update bone transform to world
					FTransform BoneTransform = SpaceBases[BoneIndex] * CurrentLocalToWorld;
						
					// move body
					BodyInst->SetBodyTransform(BoneTransform, bTeleport);
						
					// now update scale
					// if uniform, we'll use BoneTranform
					if ( MeshScale3D.IsUniform() )
					{
						// @todo UE4 should we update scale when it's simulated?
						BodyInst->UpdateBodyScale(BoneTransform.GetScale3D());						
					}
					else
					{
						// @note When you have non-uniform scale on mesh base,
						// hierarchical bone transform can update scale too often causing performance issue
						// So we just use mesh scale for all bodies when non-uniform
						// This means physics representation won't be accurate, but
						// it is performance friendly by preventing too frequent physics update
						BodyInst->UpdateBodyScale(MeshScale3D);
					}
				}
			}
			else
			{
				//make sure you have physics weight or blendphysics on, otherwise, you'll have inconsistent representation of bodies
				// @todo make this to be kismet log? But can be too intrusive
				if (!bBlendPhysics && BodyInst->PhysicsBlendWeight <= 0.f && BodyInst->BodySetup.IsValid())
				{
					UE_LOG(LogPhysics, Warning, TEXT("%s(Mesh %s, PhysicsAsset %s, Bone %s) is simulating, but no blending. "), 
						*GetName(), *GetNameSafe(SkeletalMesh), *GetNameSafe(PhysicsAsset), *BodyInst->BodySetup.Get()->BoneName.ToString());
				}
			}
		}
	}

}



void USkeletalMeshComponent::UpdateRBJointMotors()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateRBJoints);

	// moved this flag to here, so that
	// you can call it but still respect the flag
	if( !bUpdateJointsFromAnimation )
	{
		return;
	}

	const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && Constraints.Num() > 0 && SkeletalMesh)
	{
		check( PhysicsAsset->ConstraintSetup.Num() == Constraints.Num() );


		// Iterate over the constraints.
		for(int32 i=0; i<Constraints.Num(); i++)
		{
			UPhysicsConstraintTemplate* CS = PhysicsAsset->ConstraintSetup[i];
			FConstraintInstance* CI = Constraints[i];

			FName JointName = CS->DefaultInstance.JointName;
			int32 BoneIndex = SkeletalMesh->RefSkeleton.FindBoneIndex(JointName);

			// If we found this bone, and a visible bone that is not the root, and its joint is motorised in some way..
			if( (BoneIndex != INDEX_NONE) && (BoneIndex != 0) &&
				(BoneVisibilityStates[BoneIndex] == BVS_Visible) &&
				(CI->bAngularOrientationDrive) )
			{
				check(BoneIndex < LocalAtoms.Num());

				// If we find the joint - get the local-space animation between this bone and its parent.
				FQuat LocalQuat = LocalAtoms[BoneIndex].GetRotation();
				FQuatRotationTranslationMatrix LocalRot(LocalQuat, FVector::ZeroVector);

				// We loop from the graphics parent bone up to the bone that has the body which the joint is attached to, to calculate the relative transform.
				// We need this to compensate for welding, where graphics and physics parents may not be the same.
				FMatrix ControlBodyToParentBoneTM = FMatrix::Identity;

				int32 TestBoneIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex); // This give the 'graphics' parent of this bone
				bool bFoundControlBody = (SkeletalMesh->RefSkeleton.GetBoneName(TestBoneIndex) == CS->DefaultInstance.ConstraintBone2); // ConstraintBone2 is the 'physics' parent of this joint.

				while(!bFoundControlBody)
				{
					// Abort if we find a bone scaled to zero.
					const FVector Scale3D = LocalAtoms[TestBoneIndex].GetScale3D();
					const float ScaleSum = Scale3D.X + Scale3D.Y + Scale3D.Z;
					if(ScaleSum < KINDA_SMALL_NUMBER)
					{
						break;
					}

					// Add the current animated local transform into the overall controlling body->parent bone TM
					FMatrix RelTM = LocalAtoms[TestBoneIndex].ToMatrixNoScale();
					RelTM.SetOrigin(FVector::ZeroVector);
					ControlBodyToParentBoneTM = ControlBodyToParentBoneTM * RelTM;

					// Move on to parent
					TestBoneIndex = SkeletalMesh->RefSkeleton.GetParentIndex(TestBoneIndex);

					// If we are at the root - bail out.
					if(TestBoneIndex == 0)
					{
						break;
					}

					// See if this is the controlling body
					bFoundControlBody = (SkeletalMesh->RefSkeleton.GetBoneName(TestBoneIndex) == CS->DefaultInstance.ConstraintBone2);
				}

				// If after that we didn't find a parent body, we can' do this, so skip.
				if(bFoundControlBody)
				{
					// The animation rotation is between the two bodies. We need to supply the joint with the relative orientation between
					// the constraint ref frames. So we work out each body->joint transform

					FMatrix Body1TM = CS->DefaultInstance.GetRefFrame(EConstraintFrame::Frame1).ToMatrixNoScale();
					Body1TM.SetOrigin(FVector::ZeroVector);
					FMatrix Body1TMInv = Body1TM.Inverse();

					FMatrix Body2TM = CS->DefaultInstance.GetRefFrame(EConstraintFrame::Frame2).ToMatrixNoScale();
					Body2TM.SetOrigin(FVector::ZeroVector);
					FMatrix Body2TMInv = Body2TM.Inverse();

					FMatrix JointRot = Body1TM * (LocalRot * ControlBodyToParentBoneTM) * Body2TMInv;
					FQuat JointQuat(JointRot);

					// Then pass new quaternion to the joint!
					CI->SetAngularOrientationTarget(JointQuat);
				}
			}
		}
	}
}

