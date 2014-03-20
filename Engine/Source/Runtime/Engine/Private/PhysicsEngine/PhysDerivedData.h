// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX && WITH_EDITOR

#include "DerivedDataPluginInterface.h"
#include "DerivedDataCacheInterface.h"
#include "TargetPlatform.h"


#include "PhysXSupport.h"


//////////////////////////////////////////////////////////////////////////
// PhysX Cooker
class FDerivedDataPhysXCooker : public FDerivedDataPluginInterface
{
private:

	UBodySetup* BodySetup;
	UObject* CollisionDataProvider;
	FName Format;
	bool bGenerateNormalMesh;
	bool bGenerateMirroredMesh;	
	const class IPhysXFormat* Cooker;
	FGuid DataGuid;
	FString MeshId;

public:
	FDerivedDataPhysXCooker( FName InFormat, UBodySetup* InBodySetup );
	// This constructor only used by ULandscapeMeshCollisionComponent, which always only build TriMesh, not Convex...
	FDerivedDataPhysXCooker( FName InFormat, ULandscapeMeshCollisionComponent* InMeshCollision, bool bMirrored );

	virtual const TCHAR* GetPluginName() const OVERRIDE
	{
		return TEXT("PhysX");
	}

	virtual const TCHAR* GetVersionString() const OVERRIDE
	{
		// This is a version string that mimics the old versioning scheme. If you
		// want to bump this version, generate a new guid using VS->Tools->Create GUID and
		// return it here. Ex.
		return TEXT("{1F0627AE-ABEB-4206-8D78-E16BEB5DDC7E}");
	}

	virtual FString GetPluginSpecificCacheKeySuffix() const OVERRIDE
	{
		enum { UE_PHYSX_DERIVEDDATA_VER = 1 };

		const uint16 PhysXVersion = ((PX_PHYSICS_VERSION_MAJOR  & 0xF) << 12) |
				((PX_PHYSICS_VERSION_MINOR  & 0xF) << 8) |
				((PX_PHYSICS_VERSION_BUGFIX & 0xF) << 4) |
				((UE_PHYSX_DERIVEDDATA_VER	& 0xF));

		return FString::Printf( TEXT("%s_%s_%s_%d_%d_%hu_%hu"),
			*Format.ToString(),
			*DataGuid.ToString(),
			*MeshId,
			(int32)bGenerateNormalMesh,
			(int32)bGenerateMirroredMesh,
			PhysXVersion,
			Cooker ? Cooker->GetVersion( Format ) : 0xffff
			);
	}


	virtual bool IsBuildThreadsafe() const OVERRIDE
	{
		return false;
	}

	virtual bool Build( TArray<uint8>& OutData ) OVERRIDE;

	/** Return true if we can build **/
	bool CanBuild()
	{
		return !!Cooker;
	}
private:

	void InitCooker();
	int32 BuildConvex( TArray<uint8>& OutData, bool InMirrored );
	bool BuildTriMesh( TArray<uint8>& OutData, bool InMirrored, bool InUseAllTriData );
	bool ShouldGenerateTriMeshData(bool InUseAllTriData);
	bool ShouldGenerateNegXTriMeshData();
};

#endif	//WITH_PHYSX && WITH_EDITOR