// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "BoxReflectionCaptureComponent.generated.h"

	// -> will be exported to EngineDecalClasses.h
UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent), HeaderGroup=Decal, MinimalAPI)
class UBoxReflectionCaptureComponent : public UReflectionCaptureComponent
{
	GENERATED_UCLASS_BODY()

	/** Adjust capture transition distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReflectionCapture, meta=(UIMin = "1", UIMax = "1000"))
	float BoxTransitionDistance;

	UPROPERTY()
	class UBoxComponent* PreviewInfluenceBox;

	UPROPERTY()
	class UBoxComponent* PreviewCaptureBox;

public:
	virtual void UpdatePreviewShape();
	virtual float GetInfluenceBoundingRadius() const;

	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	// End UObject Interface
};

