// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavCollision.generated.h"

USTRUCT()
struct FNavCollisionCylinder
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Cylinder)
	FVector Offset;

	UPROPERTY(EditAnywhere, Category=Cylinder)
	float Radius;

	UPROPERTY(EditAnywhere, Category=Cylinder)
	float Height;
};

USTRUCT()
struct FNavCollisionBox
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Box)
	FVector Offset;

	UPROPERTY(EditAnywhere, Category=Box)
	FVector Extent;
};

struct FNavCollisionConvex
{
	TNavStatArray<FVector> VertexBuffer;
	TNavStatArray<int32> IndexBuffer;
};

UCLASS(config=Engine)
class ENGINE_API UNavCollision : public UObject
{
	GENERATED_UCLASS_BODY()

	FNavCollisionConvex TriMeshCollision;
	FNavCollisionConvex ConvexCollision;
	TNavStatArray<int32> ConvexShapeIndices;

	/** list of nav collision cylinders */
	UPROPERTY(EditAnywhere, Category=Navigation)
	TArray<FNavCollisionCylinder> CylinderCollision;

	/** list of nav collision boxes */
	UPROPERTY(EditAnywhere, Category=Navigation)
	TArray<FNavCollisionBox> BoxCollision;

	/** navigation area type (empty = default obstacle) */
	UPROPERTY(EditAnywhere, Category=Navigation)
	TSubclassOf<class UNavArea> AreaClass;

	/** If set, mesh will be used as dynamic obstacle (don't create navmesh on top, much faster adding/removing) */
	UPROPERTY(EditAnywhere, Category=Navigation, config)
	uint32 bIsDynamicObstacle : 1;

	/** If set, convex collisions will be exported offline for faster runtime navmesh building (increases memory usage) */
	UPROPERTY(EditAnywhere, Category=Navigation, config)
	uint32 bGatherConvexGeometry : 1;

	/** convex collisions are ready to use */
	uint32 bHasConvexGeometry : 1;

	/** Guid of associated BodySetup */
	FGuid BodySetupGuid;

	/** Cooked data for each format */
	FFormatContainer CookedFormatData;

	// Begin UObject interface.
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	virtual void PostLoad() OVERRIDE;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) OVERRIDE;
	// End UObject interface.

	FGuid GetGuid() const;

	/** Tries to read data from DDC, and if that fails gathers navigation
	 *	collision data, stores it and uploads to DDC */
	void Setup(class UBodySetup* BodySetup);

	/** show cylinder and box collisions */
	void DrawSimpleGeom(class FPrimitiveDrawInterface* PDI, const FTransform& Transform, const FColor Color);

	/** Get data for dynamic obstacle */
	void GetNavigationModifier(struct FCompositeNavModifier& Modifier, const FTransform& LocalToWorld) const;

	/** Read collisions data */
	bool GatherCollision();

protected:
	void ClearCollision();

#if WITH_EDITOR
	void InvalidatePhysicsData();
#endif // WITH_EDITOR
	FByteBulkData* GetCookedData(FName Format);
};
