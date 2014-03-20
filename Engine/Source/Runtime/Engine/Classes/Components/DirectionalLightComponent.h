// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "DirectionalLightComponent.generated.h"

/**
 * A light component that has parallel rays. Will provide a uniform lighting across any affected surface (eg. The Sun). This will affect all objects in the defined light-mass importance volume.
 */
UCLASS(HeaderGroup=Light, ClassGroup=Lights, hidecategories=(Object, LightProfiles), dependson=UEngineTypes, editinlinenew, meta=(BlueprintSpawnableComponent), MinimalAPI)
class UDirectionalLightComponent : public ULightComponent
{
	GENERATED_UCLASS_BODY()

	/** Whether to occlude fog and atmosphere inscattering with screenspace blurred occlusion from this light. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightShafts)
	uint32 bEnableLightShaftOcclusion:1;

	/** 
	 * Controls how dark the occlusion masking is, a value of 1 results in no darkening term.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightShafts, meta=(UIMin = "0", UIMax = "1"))
	float OcclusionMaskDarkness;

	/** Everything closer to the camera than this distance will occlude light shafts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightShafts, meta=(UIMin = "0", UIMax = "500000"))
	float OcclusionDepthRange;

	/** 
	 * Can be used to make light shafts come from somewhere other than the light's actual direction. 
	 * This will only be used when non-zero.  It does not have to be normalized.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LightShafts)
	FVector LightShaftOverrideDirection;

	UPROPERTY()
	float WholeSceneDynamicShadowRadius_DEPRECATED;

	/** 
	 * How far Cascaded Shadow Map dynamic shadows will cover for a movable light, measured from the camera.
	 * A value of 0 disables the dynamic shadow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CascadedShadowMaps, meta=(UIMin = "0", UIMax = "20000", DisplayName = "Dynamic Shadow Distance MovableLight"))
	float DynamicShadowDistanceMovableLight;

	/** 
	 * How far Cascaded Shadow Map dynamic shadows will cover for a stationary light, measured from the camera.
	 * A value of 0 disables the dynamic shadow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CascadedShadowMaps, meta=(UIMin = "0", UIMax = "20000", DisplayName = "Dynamic Shadow Distance StationaryLight"))
	float DynamicShadowDistanceStationaryLight;

	/** 
	 * Number of cascades to split the view frustum into for the whole scene dynamic shadow.  
	 * More cascades result in better shadow resolution, but adds significant rendering cost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CascadedShadowMaps, meta=(UIMin = "0", UIMax = "4", DisplayName = "Num Dynamic Shadow Cascades"))
	int32 DynamicShadowCascades;

	/** 
	 * Controls whether the cascades are distributed closer to the camera (larger exponent) or further from the camera (smaller exponent).
	 * An exponent of 1 means that cascade transitions will happen at a distance proportional to their resolution.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CascadedShadowMaps, meta=(UIMin = "1", UIMax = "4"))
	float CascadeDistributionExponent;

	/** 
	 * Proportion of the fade region between cascades.
	 * Pixels within the fade region of two cascades have their shadows blended to avoid hard transitions between quality levels.
	 * A value of zero eliminates the fade region, creating hard transitions.
	 * Higher values increase the size of the fade region, creating a more gradual transition between cascades.
	 * The value is expressed as a percentage proportion (i.e. 0.1 = 10% overlap).
	 * Ideal values are the smallest possible which still hide the transition.
	 * An increased fade region size causes an increase in shadow rendering cost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CascadedShadowMaps, meta=(UIMin = "0", UIMax = "0.3"))
	float CascadeTransitionFraction;

	/** 
	 * Controls the size of the fade out region at the far extent of the dynamic shadow's influence.  
	 * This is specified as a fraction of DynamicShadowDistance. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CascadedShadowMaps, meta=(UIMin = "0", UIMax = "1.0"))
	float ShadowDistanceFadeoutFraction;

	/** 
	 * Stationary lights only: Whether to use per-object inset shadows for movable components, even though cascaded shadow maps are enabled.
	 * This allows dynamic objects to have a shadow even when they are outside of the cascaded shadow map, which is important when DynamicShadowDistanceStationaryLight is small.
	 * If DynamicShadowDistanceStationaryLight is large (currently > 8000), this will be forced off.
	 * Disabling this can reduce shadowing cost significantly with many movable objects.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=CascadedShadowMaps)
	uint32 bUseInsetShadowsForMovableObjects : 1;

	/** The Lightmass settings for this object. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, meta=(ShowOnlyInnerProperties))
	struct FLightmassDirectionalLightSettings LightmassSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light)
	uint32 bUsedAsAtmosphereSunLight : 1;

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetDynamicShadowDistanceMovableLight(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetDynamicShadowDistanceStationaryLight(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetDynamicShadowCascades(int32 NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetCascadeDistributionExponent(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetCascadeTransitionFraction(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetShadowDistanceFadeoutFraction(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetEnableLightShaftOcclusion(bool bNewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetOcclusionMaskDarkness(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetLightShaftOverrideDirection(FVector NewValue);

	// ULightComponent interface.
	virtual FVector4 GetLightPosition() const;
	virtual ELightComponentType GetLightType() const;
	virtual FLightSceneProxy* CreateSceneProxy() const OVERRIDE;
	virtual bool IsUsedAsAtmosphereSunLight() const OVERRIDE
	{
		return bUsedAsAtmosphereSunLight;
	}

	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	virtual bool CanEditChange(const UProperty* InProperty) const OVERRIDE;
#endif // WITH_EDITOR
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	// Begin UObject Interface

	virtual void InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) OVERRIDE;
};



