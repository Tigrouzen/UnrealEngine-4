// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxSkeletalMeshImportData.generated.h"

/**
 * Import data and options used when importing a static mesh from fbx
 */
UCLASS()
class UFbxSkeletalMeshImportData : public UFbxMeshImportData
{
	GENERATED_UCLASS_BODY()

	/** Enable this option to update Skeleton (of the mesh)'s reference pose. Mesh's reference pose is always updated.  */
	UPROPERTY(EditAnywhere, Category=ImportSettings, meta=(ToolTip="If enabled, update the Skeleton (of the mesh being imported)'s reference pose."))
	uint32 bUpdateSkeletonReferencePose:1;

	/** Enable this option to use frame 0 as reference pose */
	UPROPERTY(EditAnywhere, config, Category=ImportSettings)
	uint32 bUseT0AsRefPose:1;

	/** If checked, triangles with non-matching smoothing groups will be physically split. */
	UPROPERTY(EditAnywhere, config, Category=ImportSettings)
	uint32 bPreserveSmoothingGroups:1;

	/** If checked, meshes nested in bone hierarchies will be imported instead of being converted to bones. */
	UPROPERTY(EditAnywhere, Category=ImportSettings)
	uint32 bImportMeshesInBoneHierarchy:1;

	/** True to import morph target meshes from the FBX file */
	UPROPERTY(EditAnywhere, config, Category=ImportSettings, meta=(ToolTip="If enabled, creates Unreal morph objects for the imported meshes"))
	uint32 bImportMorphTargets:1;

	/** If checked, do not filter same vertices. Keep all vertices even if they have exact same properties*/
	UPROPERTY()
	uint32 bKeepOverlappingVertices:1;

	/** Gets or creates fbx import data for the specified skeletal mesh */
	static UFbxSkeletalMeshImportData* GetImportDataForSkeletalMesh(USkeletalMesh* SkeletalMesh, UFbxSkeletalMeshImportData* TemplateForCreation);
};