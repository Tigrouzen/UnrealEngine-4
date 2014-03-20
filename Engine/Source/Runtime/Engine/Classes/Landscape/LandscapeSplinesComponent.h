// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "LandscapeSplinesComponent.generated.h"

UCLASS(HeaderGroup=Terrain,MinimalAPI)
class ULandscapeSplinesComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** Resolution of the spline, in distance per point */
	UPROPERTY()
	float SplineResolution;

	/** Color to use to draw the splines */
	UPROPERTY()
	FColor SplineColor;

	/** Sprite used to draw control points */
	UPROPERTY()
	class UTexture2D* ControlPointSprite;

	/** Mesh used to draw splines that have no mesh */
	UPROPERTY()
	class UStaticMesh* SplineEditorMesh;

	/** Whether we are in-editor and showing spline editor meshes */
	UPROPERTY(NonTransactional, Transient)
	uint32 bShowSplineEditorMesh:1;
#endif

	UPROPERTY(TextExportTransient)
	TArray<class ULandscapeSplineControlPoint*> ControlPoints;

	UPROPERTY(TextExportTransient)
	TArray<class ULandscapeSplineSegment*> Segments;


public:
	bool ModifySplines(bool bAlwaysMarkDirty = true);

#if WITH_EDITOR
	virtual void ShowSplineEditorMesh(bool bShow);
#endif

	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	// End UObject Interface

	// Begin UActorComponent interface
	virtual void OnRegister() OVERRIDE;
	virtual void OnUnregister() OVERRIDE;
	// End UActorComponent interface
	
	// Begin UPrimitiveComponent interface.
#if WITH_EDITOR
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
#endif
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const OVERRIDE;
	// End UPrimitiveComponent interface.
};
