// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// TetrahedronBuilder: Builds an octahedron (not tetrahedron) - experimental.
//=============================================================================

#pragma once
#include "TetrahedronBuilder.generated.h"

UCLASS(MinimalAPI, autoexpandcategories=BrushSettings, EditInlineNew, meta=(DisplayName="Sphere"))
class UTetrahedronBuilder : public UEditorBrushBuilder
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(ClampMin = "0.000001"))
	float Radius;

	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(ClampMin = "1", ClampMax = "5",DisplayName="Tessellation"))
	int32 SphereExtrapolation;

	UPROPERTY()
	FName GroupName;


	// Begin UBrushBuilder Interface
	virtual bool Build( UWorld* InWorld, ABrush* InBrush = NULL ) OVERRIDE;
	// End UBrushBuilder Interface

	// @todo document
	virtual void Extrapolate( int32 a, int32 b, int32 c, int32 Count, float InRadius );

	// @todo document
	virtual void BuildTetrahedron( float R, int32 InSphereExtrapolation );
};



