// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LandscapeLayerInfoObject.h"

#include "LandscapeComponent.generated.h"

class FLandscapeComponentDerivedData
{
	/** The compressed Landscape component data for mobile rendering. Serialized to disk. 
	    On device, freed once it has been decompressed. */
	TArray<uint8> CompressedLandscapeData;

public:
	/** Returns true if there is any valid platform data */
	bool HasValidPlatformData() const
	{
		return CompressedLandscapeData.Num() != 0;
	}

	/** Initializes the compressed data from an uncompressed source. */
	void InitializeFromUncompressedData(const TArray<uint8>& UncompressedData);

	/** Decompresses and returns the data. Also frees the compressed data from memory when running with cooked data */
	void GetUncompressedData(TArray<uint8>& OutUncompressedData);

	/** Constructs a key string for the DDC that uniquely identifies a the Landscape component's derived data. */
	static FString GetDDCKeyString(const FGuid& StateId);

	/** Loads the platform data from DDC */
	bool LoadFromDDC(const FGuid& StateId);

	/** Saves the compressed platform data to the DDC */
	void SaveToDDC(const FGuid& StateId);

	/* Serializer */
	friend FArchive& operator<<(FArchive& Ar, FLandscapeComponentDerivedData& Data);
};

/** Stores information about which weightmap texture and channel each layer is stored */
USTRUCT()
struct FWeightmapLayerAllocationInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName LayerName_DEPRECATED;

	UPROPERTY()
	ULandscapeLayerInfoObject* LayerInfo;

	UPROPERTY()
	uint8 WeightmapTextureIndex;

	UPROPERTY()
	uint8 WeightmapTextureChannel;


	FWeightmapLayerAllocationInfo()
		: WeightmapTextureIndex(0)
		, WeightmapTextureChannel(0)
	{
	}


	FWeightmapLayerAllocationInfo(ULandscapeLayerInfoObject* InLayerInfo)
		:	LayerInfo(InLayerInfo)
		,	WeightmapTextureIndex(255)	// Indicates an invalid allocation
		,	WeightmapTextureChannel(255)
	{
	}
	
	FName GetLayerName() const
	{
		if (LayerInfo)
		{
			return LayerInfo->LayerName;
		}
		return NAME_None;
	}
};

UCLASS(HeaderGroup=Terrain, hidecategories=(Display, Attachment, Physics, Debug, Collision, Movement, Rendering, PrimitiveComponent, Object, Transform), MinimalAPI)
class ULandscapeComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 SectionBaseX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 SectionBaseY;

	/** Total number of quads for this component */
	UPROPERTY()
	int32 ComponentSizeQuads;

	/** Number of quads for a subsection of the component. SubsectionSizeQuads+1 must be a power of two. */
	UPROPERTY()
	int32 SubsectionSizeQuads;

	/** Number of subsections in X or Y axis */
	UPROPERTY()
	int32 NumSubsections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LandscapeComponent)
	class UMaterialInterface* OverrideMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LandscapeComponent)
	class UMaterialInterface* OverrideHoleMaterial;

	UPROPERTY(TextExportTransient)
	class UMaterialInstanceConstant* MaterialInstance;

	/** List of layers, and the weightmap and channel they are stored */
	UPROPERTY()
	TArray<struct FWeightmapLayerAllocationInfo> WeightmapLayerAllocations;

	/** Weightmap texture reference */
	UPROPERTY(TextExportTransient)
	TArray<class UTexture2D*> WeightmapTextures;

	/** XYOffsetmap texture reference */
	UPROPERTY(TextExportTransient)
	class UTexture2D* XYOffsetmapTexture;

	/** UV offset to component's weightmap data from component local coordinates*/
	UPROPERTY()
	FVector4 WeightmapScaleBias;

	/** U or V offset into the weightmap for the first subsection, in texture UV space */
	UPROPERTY()
	float WeightmapSubsectionOffset;

	/** UV offset to Heightmap data from component local coordinates */
	UPROPERTY()
	FVector4 HeightmapScaleBias;

	/** Heightmap texture reference */
	UPROPERTY(TextExportTransient)
	class UTexture2D* HeightmapTexture;

	/** Cached bounds, created at heightmap update time */
	UPROPERTY()
	FBoxSphereBounds CachedBoxSphereBounds_DEPRECATED;

	/** Cached local-space bounding box, created at heightmap update time */
	UPROPERTY()
	FBox CachedLocalBox;

	/** Reference to associated collision component */
	UPROPERTY()
	TLazyObjectPtr<class ULandscapeHeightfieldCollisionComponent> CollisionComponent;

private:
#if WITH_EDITORONLY_DATA
	/** Unique ID for this component, used for caching during distributed lighting */
	UPROPERTY()
	FGuid LightingGuid;

#endif // WITH_EDITORONLY_DATA
public:
	/**	INTERNAL: Array of lights that don't apply to the terrain component.		*/
	UPROPERTY()
	TArray<FGuid> IrrelevantLights;

	/** Reference to the texture lightmap resource. */
	FLightMapRef LightMap;

	FShadowMapRef ShadowMap;

	/** Heightfield mipmap used to generate collision */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent)
	int32 CollisionMipLevel;

	/** StaticLightingResolution overriding per component, default value 0 means no overriding */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent)
	float StaticLightingResolution;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	uint32 bNeedPostUndo:1;
#endif // WITH_EDITORONLY_DATA

	/** Forced LOD level to use when rendering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 ForcedLOD;

	/** Neighbor LOD data to use when rendering, 255 is unspecified */
	UPROPERTY()
	uint8 NeighborLOD[8];

	/** LOD level Bias to use when rendering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 LODBias;

	/** Neighbor LOD bias to use when rendering, 128 is 0 bias, 0 is -128 bias, 255 is 127 bias */
	UPROPERTY()
	uint8 NeighborLODBias[8];

	UPROPERTY()
	FGuid StateId;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, DuplicateTransient)
	UTexture2D* SelectDataTexture; // Data texture used for selection mask

	/** Runtime-generated editor data for ES2 emulation */
	UPROPERTY(Transient, DuplicateTransient)
	UTexture2D* MobileNormalmapTexture;

	/** Runtime-generated editor data for ES2 emulation */
	UPROPERTY(Transient, DuplicateTransient)
	UMaterialInterface* MobileMaterialInterface;
#endif


public:
	/** Pointer to data shared with the render therad, used by the editor tools */
	struct FLandscapeEditToolRenderData* EditToolRenderData;
	/** Platform Data where don't support texture sampling in vertex buffer */
	FLandscapeComponentDerivedData PlatformData;

	// Begin UObject interface.	
	virtual void PostInitProperties() OVERRIDE;	
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	virtual void BeginDestroy() OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void PostDuplicate(bool bDuplicateForPIE) OVERRIDE;
#if WITH_EDITOR
	virtual void PostLoad() OVERRIDE;
	virtual void PostEditUndo() OVERRIDE;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	// End UObject interface

	/** Fix up component layers, weightmaps
	 */
	ENGINE_API void FixupWeightmaps();
	
	// Begin UPrimitiveComponent interface.
	virtual bool GetLightMapResolution( int32& Width, int32& Height ) const OVERRIDE;
	virtual void GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const OVERRIDE;
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options) OVERRIDE;
#endif
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const OVERRIDE;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	virtual ELightMapInteractionType GetStaticLightingType() const OVERRIDE { return LMIT_Texture;	}
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const OVERRIDE;

#if WITH_EDITOR
	virtual int32 GetNumMaterials() const OVERRIDE;
	virtual class UMaterialInterface* GetMaterial(int32 ElementIndex) const OVERRIDE;
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* Material) OVERRIDE;
#endif
	// End UPrimitiveComponent interface.

	// Begin USceneComponent interface.
	virtual void DestroyComponent() OVERRIDE;
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	// End USceneComponent interface.

	// Begin UActorComponent interface.
	virtual void OnRegister() OVERRIDE;
	virtual void OnUnregister() OVERRIDE;
	// End UActorComponent interface.


#if WITH_EDITOR
	/** @todo document */
	ENGINE_API class ULandscapeInfo* GetLandscapeInfo(bool bSpawnNewActor = true) const;

	/** @todo document */
	void DeleteLayer(ULandscapeLayerInfoObject* LayerInfo, struct FLandscapeEditDataInterface* LandscapeEdit);

	void ReplaceLayer(ULandscapeLayerInfoObject* FromLayerInfo, ULandscapeLayerInfoObject* ToLayerInfo, struct FLandscapeEditDataInterface* LandscapeEdit);
	
	void GeneratePlatformVertexData();
	UMaterialInstance* GeneratePlatformPixelData(TArray<class UTexture2D*>& WeightmapTextures, bool bIsCooking);
#endif

	/** @todo document */
	virtual void InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly);


	/** Get the landscape actor associated with this component. */
	class ALandscape* GetLandscapeActor() const;

	/** @todo document */
	ENGINE_API class ALandscapeProxy* GetLandscapeProxy() const;

	/** @return Component section base as FIntPoint */
	ENGINE_API FIntPoint GetSectionBase() const; 

	/** @param InSectionBase new section base for a component */
	ENGINE_API void SetSectionBase(FIntPoint InSectionBase);

	/** @todo document */
	TMap< UTexture2D*,struct FLandscapeWeightmapUsage >& GetWeightmapUsageMap();

	/** @todo document */
	const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid;
#endif // WITH_EDITORONLY_DATA
	}

	/** @todo document */
	void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = FGuid::NewGuid();
#endif // WITH_EDITORONLY_DATA
	}

#if WITH_EDITOR


	/** Initialize the landscape component */
	ENGINE_API void Init(int32 InBaseX,int32 InBaseY,int32 InComponentSizeQuads, int32 InNumSubsections,int32 InSubsectionSizeQuads);

	/**
	 * Recalculate cached bounds using height values.
	 */
	ENGINE_API void UpdateCachedBounds();

	/**
	 * Update the MaterialInstance parameters to match the layer and weightmaps for this component
	 * Creates the MaterialInstance if it doesn't exist.
	 */
	ENGINE_API void UpdateMaterialInstances();

	/** Helper function for UpdateMaterialInstance to get Material without set parameters */
	UMaterialInstanceConstant* GetCombinationMaterial(bool bMobile = false);
	/**
	 * Generate mipmaps for height and tangent data.
	 * @param HeightmapTextureMipData - array of pointers to the locked mip data.
	 *           This should only include the mips that are generated directly from this component's data
	 *           ie where each subsection has at least 2 vertices.
	* @param ComponentX1 - region of texture to update in component space, MAX_int32 meant end of X component in ALandscape::Import()
	* @param ComponentY1 - region of texture to update in component space, MAX_int32 meant end of Y component in ALandscape::Import()
	* @param ComponentX2 (optional) - region of texture to update in component space
	* @param ComponentY2 (optional) - region of texture to update in component space
	* @param TextureDataInfo - FLandscapeTextureDataInfo pointer, to notify of the mip data region updated.
	 */
	void GenerateHeightmapMips(TArray<FColor*>& HeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32,struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/**
	 * Generate empty mipmaps for weightmap
	 */
	ENGINE_API static void CreateEmptyTextureMips(UTexture2D* Texture, bool bClear = false);

	/**
	 * Generate mipmaps for weightmap
	 * Assumes all weightmaps are unique to this component.
	 * @param WeightmapTextureBaseMipData: array of pointers to each of the weightmaps' base locked mip data.
	 */
	template<typename DataType>

	/** @todo document */
	static void GenerateMipsTempl(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, DataType* BaseMipData);

	/** @todo document */
	static void GenerateWeightmapMips(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, FColor* BaseMipData);

	/**
	 * Update mipmaps for existing weightmap texture
	 */
	template<typename DataType>

	/** @todo document */
	static void UpdateMipsTempl(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<DataType*>& WeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/** @todo document */
	ENGINE_API static void UpdateWeightmapMips(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<FColor*>& WeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/** @todo document */
	static void UpdateDataMips(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* Texture, TArray<uint8*>& TextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/**
	 * Create or updatescollision component height data
	 * @param HeightmapTextureMipData: heightmap data
	 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
	 * @param Whether to update bounds from render component.
	 */
	void UpdateCollisionHeightData(const FColor* HeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, bool bUpdateBounds=false, const FColor* XYOffsetTextureMipData = NULL, bool bRebuild=false);

	/**
	 * Update collision component dominant layer data
	 * @param WeightmapTextureMipData: weightmap data
	 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
	 * @param Whether to update bounds from render component.
	 */
	void UpdateCollisionLayerData(TArray<FColor*>& WeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32);

	/**
	 * Update collision component dominant layer data for the whole component, locking and unlocking the weightmap textures.
	 */
	void UpdateCollisionLayerData();

	/**
	 * Create weightmaps for this component for the layers specified in the WeightmapLayerAllocations array
	 */
	void ReallocateWeightmaps(struct FLandscapeEditDataInterface* DataInterface=NULL);

	/** Returns the actor's LandscapeMaterial, or the Component's OverrideLandscapeMaterial if set */
	ENGINE_API UMaterialInterface* GetLandscapeMaterial() const;

	/** Returns the actor's LandscapeHoleMaterial, or the Component's OverrideLandscapeHoleMaterial if set */
	ENGINE_API UMaterialInterface* GetLandscapeHoleMaterial() const;

	/** Returns true if this component has visibility painted */
	ENGINE_API bool ComponentHasVisibilityPainted() const;

	/**
	 * Generate a key for this component's layer allocations to use with MaterialInstanceConstantMap.
	 */
	FString GetLayerAllocationKey(bool bMobile = false) const;

	/** @todo document */
	void GetLayerDebugColorKey(int32& R, int32& G, int32& B) const;

	/** @todo document */
	void RemoveInvalidWeightmaps();

	/** @todo document */
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent);

	/** @todo document */
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn);

	/** @todo document */
	ENGINE_API void InitHeightmapData(TArray<FColor>& Heights, bool bUpdateCollision);

	/** @todo document */
	ENGINE_API void InitWeightmapData(TArray<class ULandscapeLayerInfoObject*>& LayerInfos, TArray<TArray<uint8> >& Weights);

	/** @todo document */
	ENGINE_API float GetLayerWeightAtLocation( const FVector& InLocation, ULandscapeLayerInfoObject* LayerInfo, TArray<uint8>* LayerCache=NULL );

	/** Extends passed region with this component section size */
	ENGINE_API void GetComponentExtent(int32& MinX, int32& MinY, int32& MaxX, int32& MaxY) const;
#endif

	friend class FLandscapeComponentSceneProxy;
	friend struct FLandscapeComponentDataInterface;

	void SetLOD(bool bForced, int32 InLODValue);
};



