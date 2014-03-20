// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FSignedDistanceFieldShadowSample
{
	/** Normalized and encoded distance to the nearest shadow transition, in the range [0, 1], where .5 is at the transition. */
	float Distance;
	/** Normalized penumbra size, in the range [0,1]. */
	float PenumbraSize;

	/** True if this sample maps to a valid point on a surface. */
	bool IsMapped;
};

struct FQuantizedSignedDistanceFieldShadowSample
{
	enum {NumFilterableComponents = 2};
	uint8 Distance;
	uint8 PenumbraSize;
	uint8 Coverage;

	float GetFilterableComponent(int32 Index) const
	{
		if (Index == 0)
		{
			return Distance / 255.0f;
		}
		else
		{
			checkSlow(Index == 1);
			return PenumbraSize / 255.0f;
		}
	}

	void SetFilterableComponent(float InComponent, int32 Index)
	{
		if (Index == 0)
		{
			Distance = (uint8)FMath::Clamp<int32>(FMath::Trunc(InComponent * 255.0f),0,255);
		}
		else
		{
			checkSlow(Index == 1);
			PenumbraSize = (uint8)FMath::Clamp<int32>(FMath::Trunc(InComponent * 255.0f),0,255);
		}
	}

	/** Equality operator */
	bool operator==( const FQuantizedSignedDistanceFieldShadowSample& RHS ) const
	{
		return Distance == RHS.Distance &&
			   PenumbraSize == RHS.PenumbraSize &&
			   Coverage == RHS.Coverage;
	}

	FQuantizedSignedDistanceFieldShadowSample() {}
	FQuantizedSignedDistanceFieldShadowSample(const FSignedDistanceFieldShadowSample& InSample)
	{
		Distance = (uint8)FMath::Clamp<int32>(FMath::Trunc(InSample.Distance * 255.0f),0,255);
		PenumbraSize = (uint8)FMath::Clamp<int32>(FMath::Trunc(InSample.PenumbraSize * 255.0f),0,255);
		Coverage = InSample.IsMapped ? 255 : 0;
	}
};

/**
 * The raw data which is used to construct a 2D shadowmap.
 */
class FShadowMapData2D
{
public:
	virtual ~FShadowMapData2D() {}

	// Accessors.
	uint32 GetSizeX() const { return SizeX; }
	uint32 GetSizeY() const { return SizeY; }

	// USurface interface
	virtual float GetSurfaceWidth() const { return SizeX; }
	virtual float GetSurfaceHeight() const { return SizeY; }

	enum ShadowMapDataType {
		UNKNOWN,
		SHADOW_FACTOR_DATA,
		SHADOW_FACTOR_DATA_QUANTIZED,
		SHADOW_SIGNED_DISTANCE_FIELD_DATA,
		SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED,
	};
	virtual ShadowMapDataType GetType() const { return UNKNOWN; }

	virtual void Quantize( TArray<struct FQuantizedShadowSample>& OutData ) const {}
	virtual void Quantize( TArray<struct FQuantizedSignedDistanceFieldShadowSample>& OutData ) const {}

	virtual void Serialize( FArchive* OutShadowMap ) {}

protected:

	FShadowMapData2D(uint32 InSizeX,uint32 InSizeY):
		SizeX(InSizeX),
		SizeY(InSizeY)
	{
	}

	/** The width of the shadow-map. */
	uint32 SizeX;

	/** The height of the shadow-map. */
	uint32 SizeY;
};

/**
 * A 2D signed distance field map, which consists of FSignedDistanceFieldShadowSample's.
 */
class FShadowSignedDistanceFieldData2D : public FShadowMapData2D
{
public:

	FShadowSignedDistanceFieldData2D(uint32 InSizeX,uint32 InSizeY)
	:	FShadowMapData2D(InSizeX, InSizeY)
	{
		Data.Empty(SizeX * SizeY);
		Data.AddZeroed(SizeX * SizeY);
	}

	const FSignedDistanceFieldShadowSample& operator()(uint32 X,uint32 Y) const { return Data[SizeX * Y + X]; }
	FSignedDistanceFieldShadowSample& operator()(uint32 X,uint32 Y) { return Data[SizeX * Y + X]; }

	virtual ShadowMapDataType GetType() const { return SHADOW_SIGNED_DISTANCE_FIELD_DATA; }
	virtual void Quantize( TArray<struct FQuantizedSignedDistanceFieldShadowSample>& OutData ) const
	{
		OutData.Empty( Data.Num() );
		OutData.AddUninitialized( Data.Num() );
		for( int32 Index = 0; Index < Data.Num(); Index++ )
		{
			OutData[Index] = Data[Index];
		}
	}

	virtual void Serialize( FArchive* OutShadowMap )
	{
		for( int32 Index = 0; Index < Data.Num(); Index++ )
		{
			OutShadowMap->Serialize(&Data[Index], sizeof(FSignedDistanceFieldShadowSample));
		}
	}

private:
	TArray<FSignedDistanceFieldShadowSample> Data;
};

/**
 * A 2D signed distance field map, which consists of FQuantizedSignedDistanceFieldShadowSample's.
 */
class FQuantizedShadowSignedDistanceFieldData2D : public FShadowMapData2D
{
public:

	FQuantizedShadowSignedDistanceFieldData2D(uint32 InSizeX,uint32 InSizeY)
	:	FShadowMapData2D(InSizeX, InSizeY)
	{
		Data.Empty(SizeX * SizeY);
		Data.AddZeroed(SizeX * SizeY);
	}

	const FQuantizedSignedDistanceFieldShadowSample& operator()(uint32 X,uint32 Y) const { return Data[SizeX * Y + X]; }
	FQuantizedSignedDistanceFieldShadowSample& operator()(uint32 X,uint32 Y) { return Data[SizeX * Y + X]; }

	virtual ShadowMapDataType GetType() const { return SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED; }
	virtual void Quantize( TArray<struct FQuantizedSignedDistanceFieldShadowSample>& OutData ) const
	{
		OutData.Empty( Data.Num() );
		OutData.AddUninitialized( Data.Num() );
		for( int32 Index = 0; Index < Data.Num(); Index++ )
		{
			OutData[Index] = Data[Index];
		}
	}

	virtual void Serialize( FArchive* OutShadowMap )
	{
		for( int32 Index = 0; Index < Data.Num(); Index++ )
		{
			OutShadowMap->Serialize(&Data[Index], sizeof(FQuantizedSignedDistanceFieldShadowSample));
		}
	}

private:
	TArray<FQuantizedSignedDistanceFieldShadowSample> Data;
};

class FFourDistanceFieldSamples
{
public:
	FQuantizedSignedDistanceFieldShadowSample Samples[4];
};

/**
 * The abstract base class of 1D and 2D shadow-maps.
 */
class FShadowMap : private FDeferredCleanupInterface
{
public:
	enum
	{
		SMT_None = 0,
		SMT_2D = 2,
	};

	/** The GUIDs of lights which this shadow-map stores. */
	TArray<FGuid> LightGuids;

	/** Default constructor. */
	FShadowMap() : 
		NumRefs(0) 
	{
	}

	/** Destructor. */
	virtual ~FShadowMap() { check(NumRefs == 0); }

	/**
	 * Checks if a light is stored in this shadow-map.
	 * @param	LightGuid - The GUID of the light to check for.
	 * @return	True if the light is stored in the light-map.
	 */
	bool ContainsLight(const FGuid& LightGuid) const
	{
		return LightGuids.Find(LightGuid) != INDEX_NONE;
	}

	// FShadowMap interface.
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) {}
	virtual void Serialize(FArchive& Ar);
	virtual FShadowMapInteraction GetInteraction() const = 0;

	// Runtime type casting.
	virtual class FShadowMap2D* GetShadowMap2D() { return NULL; }
	virtual const FShadowMap2D* GetShadowMap2D() const { return NULL; }

	// Reference counting.
	void AddRef()
	{
		check( IsInGameThread() );
		NumRefs++;
	}
	void Release()
	{
		check( IsInGameThread() );
		checkSlow(NumRefs > 0);
		if(--NumRefs == 0)
		{
			Cleanup();
		}
	}

protected:

	/**
	 * Called when the light-map is no longer referenced.  Should release the lightmap's resources.
	 */
	ENGINE_API virtual void Cleanup();

private:
	int32 NumRefs;
	
	// FDeferredCleanupInterface
	ENGINE_API virtual void FinishCleanup();
};

class FShadowMap2D : public FShadowMap
{
public:

	/**
	 * Executes all pending shadow-map encoding requests.
	 * @param	InWorld				World in which the textures exist
	 * @param	bLightingSuccessful	Whether the lighting build was successful or not.	 
	 */
	ENGINE_API static void EncodeTextures( UWorld* InWorld, bool bLightingSuccessful );

	/**
	 * Constructs mip maps for a single shadowmap texture.
	 */
	static void EncodeSingleTexture(struct FShadowMapPendingTexture& PendingTexture, UShadowMapTexture2D* Texture, TArray< TArray<FFourDistanceFieldSamples> >& MipData);

	static FShadowMap2D* AllocateShadowMap(
		UObject* LightMapOuter, 
		const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData,
		const FBoxSphereBounds& Bounds, 
		ELightMapPaddingType InPaddingType,
		EShadowMapFlags InShadowmapFlags);

	FShadowMap2D();

	FShadowMap2D(const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData);

	// Accessors.
	class UShadowMapTexture2D* GetTexture() const { check(IsValid()); return Texture; }
	const FVector2D& GetCoordinateScale() const { check(IsValid()); return CoordinateScale; }
	const FVector2D& GetCoordinateBias() const { check(IsValid()); return CoordinateBias; }
	bool IsValid() const { return Texture != NULL; }
	bool IsShadowFactorTexture() const { return false; }

	// FShadowMap interface.
	virtual void AddReferencedObjects(FReferenceCollector& Collector);
	virtual void Serialize(FArchive& Ar);
	virtual FShadowMapInteraction GetInteraction() const;

	// Runtime type casting.
	virtual class FShadowMap2D* GetShadowMap2D() { return this; }
	virtual const FShadowMap2D* GetShadowMap2D() const { return this; }

	/** Call to enable/disable status update of LightMap encoding */
	static void SetStatusUpdate(bool bInEnable)
	{
		bUpdateStatus = bInEnable;
	}

	static bool GetStatusUpdate()
	{
		return bUpdateStatus;
	}

private:

	/** The texture which contains the shadow-map data. */
	UShadowMapTexture2D* Texture;

	/** The scale which is applied to the shadow-map coordinates before sampling the shadow-map textures. */
	FVector2D CoordinateScale;

	/** The bias which is applied to the shadow-map coordinates before sampling the shadow-map textures. */
	FVector2D CoordinateBias;

	/** Tracks which of the 4 channels has valid texture data. */
	bool bChannelValid[4];

	/** If true, update the status when encoding light maps */
	ENGINE_API static bool bUpdateStatus;
};


/** A reference to a shadow-map. */
typedef TRefCountPtr<FShadowMap> FShadowMapRef;

/** Shadowmap reference serializer */
extern FArchive& operator<<(FArchive& Ar,FShadowMap*& R);
