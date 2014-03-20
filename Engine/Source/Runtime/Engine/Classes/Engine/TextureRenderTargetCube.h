// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "TextureRenderTargetCube.generated.h"

/**
 * TextureRenderTargetCube
 *
 * Cube render target texture resource. This can be used as a target
 * for rendering as well as rendered as a regular cube texture resource.
 *
 */
UCLASS(hidecategories=Object, hidecategories=Texture, MinimalAPI)
class UTextureRenderTargetCube : public UTextureRenderTarget
{
	GENERATED_UCLASS_BODY()

	/** The width of the texture.												*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureRenderTargetCube, AssetRegistrySearchable)
	int32 SizeX;

	/** the color the texture is cleared to */
	UPROPERTY()
	FLinearColor ClearColor;

	/** The format of the texture data.											*/
	/** Normally the format is derived from bHDR, this allows code to set the format explicitly. */
	UPROPERTY()
	TEnumAsByte<enum EPixelFormat> OverrideFormat;

	/** Whether to support storing HDR values, which requires more memory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTargetCube, AssetRegistrySearchable)
	uint32 bHDR:1;

	/** True to force linear gamma space for this render target */
	UPROPERTY()
	uint32 bForceLinearGamma:1;

	/** 
	* Initialize the settings needed to create a render target texture
	* and create its resource
	* @param	InSizeX - width of the texture
	* @param	InFormat - format of the texture
	*/
	ENGINE_API void Init(uint32 InSizeX, EPixelFormat InFormat);

	/** Initializes the render target, the format will be derived from the value of bHDR. */
	ENGINE_API void InitAutoFormat(uint32 InSizeX);

	/**
	* Utility for creating a new UTextureCube from a TextureRenderTargetCube.
	* TextureRenderTargetCube must be square and a power of two size.
	* @param	Outer			Outer to use when constructing the new TextureCube.
	* @param	NewTexName		Name of new UTextureCube object.
	* @param	Flags			Various control flags for operation (see EObjectFlags)
	* @return					New UTextureCube object.
	*/
	ENGINE_API class UTextureCube* ConstructTextureCube(UObject* Outer, const FString& NewTexName, EObjectFlags InFlags);

	// Begin UTexture interface.
	virtual float GetSurfaceWidth() const  OVERRIDE { return SizeX; }
	virtual float GetSurfaceHeight()const  OVERRIDE { return SizeX; }	
	virtual FTextureResource* CreateResource() OVERRIDE;
	virtual EMaterialValueType GetMaterialType() OVERRIDE;
	// End UTexture interface.

	EPixelFormat GetFormat() const
	{
		if(OverrideFormat == PF_Unknown)
		{
			return bHDR ? PF_FloatRGBA : PF_B8G8R8A8;
		}
		else
		{
			return OverrideFormat;
		}
	}

	FORCEINLINE int32 GetNumMips() const
	{
		return 1;
	}
	// Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostLoad() OVERRIDE;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) OVERRIDE;
	virtual FString GetDesc() OVERRIDE;
	// Begin UObject interface
};



