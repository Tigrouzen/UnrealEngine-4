// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/*=============================================================================
	Texture.h: Unreal texture related classes.
=============================================================================*/

#include "Engine/TextureDefines.h"

/** Maximum number of slices in texture source art. */
#define MAX_TEXTURE_SOURCE_SLICES 6

/** Thread-safe counter indicating the texture streaming state. The definitions below are mirrored in Texture2D.h */
enum ETextureStreamingState
{
	// The renderer hasn't created the resource yet.
	TexState_InProgress_Initialization	= -1,
	// There are no pending requests/ all requests have been fulfilled.
	TexState_ReadyFor_Requests			= 0,
	// Finalization has been kicked off and is in progress.
	TexState_InProgress_Finalization	= 1,
	// Initial request has completed and finalization needs to be kicked off.
	TexState_ReadyFor_Finalization		= 2,
	// Mip data is in the process of being uploaded to the GPU.
	TexState_InProgress_Upload			= 3,
	// Mip data has been loaded in to system memory and is ready to be transferred to the GPU.
	TexState_ReadyFor_Upload			= 4,
	// We're currently loading in mip data.
	TexState_InProgress_Loading			= 5,
	// ...
	// States 2+N means we're currently loading in N mips
	// ...
	// Memory has been allocated and we're ready to start loading in mips.
	TexState_ReadyFor_Loading			= 100,
	// We're currently allocating/preparing memory for the new mip count.
	TexState_InProgress_Allocation		= 101,
	// The RHI is asynchronously allocating/preparing memory for the new mip count.
	TexState_InProgress_AsyncAllocation = 102
};

/**
 * Async worker to stream mips from the derived data cache.
 */
class FAsyncStreamDerivedMipWorker : public FNonAbandonableTask
{
public:
	/** Initialization constructor. */
	FAsyncStreamDerivedMipWorker(
		const FString& InDerivedDataKey,
		void* InDestMipData,
		int32 InMipSize,
		FThreadSafeCounter* InThreadSafeCounter
		);
	
	/**
	 * Retrieves the derived mip from the derived data cache.
	 */
	void DoWork();

	/**
	 * Name of the async task.
	 */
	static const TCHAR* Name()
	{
		return TEXT("FAsyncStreamDerivedMipTask");
	}

	/**
	 * Returns true if the streaming mip request failed.
	 */
	bool DidRequestFail() const
	{
		return bRequestFailed;
	}

private:
	/** Key for retrieving mip data from the derived data cache. */
	FString DerivedDataKey;
	/** The location to which the mip data should be copied. */
	void* DestMipData;
	/** The size of the mip in bytes. */
	int32 ExpectedMipSize;
	/** true if the mip data was not present in the derived data cache. */
	bool bRequestFailed;
	/** Thread-safe counter to decrement when data has been copied. */
	FThreadSafeCounter* ThreadSafeCounter;
};

/** Async task to stream mips from the derived data cache. */
typedef FAsyncTask<FAsyncStreamDerivedMipWorker> FAsyncStreamDerivedMipTask;

/**
 * Task to create a texture asynchronously.
 */
class FCreateTextureTask : public FNonAbandonableTask
{
public:
	/** Task arguments. */
	struct FArguments
	{
		/** Width of the texture. */
		uint32 SizeX;
		/** Height of the texture. */
		uint32 SizeY;
		/** Format of the texture. */
		EPixelFormat Format;
		/** The number of mips. */
		uint32 NumMips;
		/** Texture creation flags. */
		uint32 Flags;
		/** Initial mip data. */
		void** MipData;
		/** The number of provided mips. */
		uint32 NumNewMips;
		/** Pointer to a reference where the new texture will be stored. */
		FTexture2DRHIRef* TextureRefPtr;
		/** Thread safe counter to decrement when complete. */
		FThreadSafeCounter* ThreadSafeCounter;
	};

	/** Initialization constructor. */
	FCreateTextureTask(FArguments InArgs)
		: Args(InArgs)
	{
		check(Args.TextureRefPtr);
		check(Args.ThreadSafeCounter);
	}

	static const TCHAR* Name()
	{
		return TEXT("FCreateTextureTask");
	}

	/** Creates the texture. */
	void DoWork();

private:
	/** Task arguments. */
	FArguments Args;
};

/** Async task to create a texture. */
typedef FAsyncTask<FCreateTextureTask> FAsyncCreateTextureTask;

/**
 * A 2D texture mip-map.
 */
struct FTexture2DMipMap
{
	/** Width of the mip-map. */
	int32 SizeX;
	/** Height of the mip-map. */
	int32 SizeY;
	/** Bulk data if stored in the package. */
	FByteBulkData BulkData;

	/** Default constructor. */
	FTexture2DMipMap()
		: SizeX(0)
		, SizeY(0)
	{
	}

	/** Serialization. */
	void Serialize(FArchive& Ar, UObject* Owner, int32 MipIndex);

#if WITH_EDITORONLY_DATA
	/** Key if stored in the derived data cache. */
	FString DerivedDataKey;

	/**
	 * Place mip-map data in the derived data cache associated with the provided
	 * key.
	 */
	void StoreInDerivedDataCache(const FString& InDerivedDataKey);
#endif // #if WITH_EDITORONLY_DATA
};

/** 
 * The rendering resource which represents a texture.
 */
class FTextureResource : public FTexture
{
public:

	FRenderCommandFence ReleaseFence;

	FTextureResource()
	{}
	virtual ~FTextureResource() {}

#if STATS
	/* The Stat_ FName corresponding to each TEXTUREGROUP */
	static FName TextureGroupStatFNames[TEXTUREGROUP_MAX];
#endif
};

/**
 * FTextureResource implementation for streamable 2D textures.
 */
class FTexture2DResource : public FTextureResource
{
public:
	/**
	 * Minimal initialization constructor.
	 *
	 * @param InOwner			UTexture2D which this FTexture2DResource represents.
	 * @param InitialMipCount	Initial number of miplevels to upload to card
	 * @param InFilename		Filename to read data from
 	 */
	FTexture2DResource( UTexture2D* InOwner, int32 InitialMipCount );

	/**
	 * Destructor, freeing MipData in the case of resource being destroyed without ever 
	 * having been initialized by the rendering thread via InitRHI.
	 */
	virtual ~FTexture2DResource();

	// FRenderResource interface.

	/**
	 * Called when the resource is initialized. This is only called by the rendering thread.
	 */
	virtual void InitRHI();
	/**
	 * Called when the resource is released. This is only called by the rendering thread.
	 */
	virtual void ReleaseRHI();

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const;

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const;

	/**
	 * Called from the game thread to kick off a change in ResidentMips after modifying RequestedMips.
	 * @param bShouldPrioritizeAsyncIORequest	- Whether the Async I/O request should have higher priority
	 */
	void BeginUpdateMipCount( bool bShouldPrioritizeAsyncIORequest );
	/**
	 * Called from the game thread to kick off async I/O to load in new mips.
	 */
	void BeginLoadMipData();
	/**
	 * Called from the game thread to kick off uploading mip data to the GPU.
	 */
	void BeginUploadMipData();
	/**
	 * Called from the game thread to kick off finalization of mip change.
	 */
	void BeginFinalizeMipCount();
	/**
	 * Called from the game thread to kick off cancelation of async operations for request.
	 */
	void BeginCancelUpdate();

	/** 
	 * Accessor
	 * @return Texture2DRHI
	 */
	FTexture2DRHIRef GetTexture2DRHI()
	{
		return Texture2DRHI;
	}

	bool DidUpdateMipCountFail() const
	{
		return NumFailedReallocs > 0 || bDerivedDataStreamRequestFailed;
	}

	bool DidDerivedDataRequestFail() const
	{
		return bDerivedDataStreamRequestFailed;
	}

	/**
	 *	Tries to reallocate the texture for a new mip count.
	 *	@param OldMipCount	- The old mip count we're currently using.
	 *	@param NewMipCount	- The new mip count to use.
	 */
	bool TryReallocate( int32 OldMipCount, int32 NewMipCount );

	virtual FString GetFriendlyName() const;

	//Returns the raw data for a particular mip level
	void* GetRawMipData( uint32 MipIndex);

	//Returns the current first mip (always valid)
	int32 GetCurrentFirstMip() const
	{
		return CurrentFirstMip;
	}

private:
	/** Texture streaming command classes that need to be friends in order to call Update/FinalizeMipCount.	*/
	friend class FUpdateMipCountCommand;
	friend class FFinalinzeMipCountCommand;
	friend class FCancelUpdateCommand;
	friend class FUploadMipDataCommand;
	friend class UTexture2D;

	/** The UTexture2D which this resource represents.														*/
	const UTexture2D*	Owner;
	/** Resource memory allocated by the owner for serialize bulk mip data into								*/
	FTexture2DResourceMem* ResourceMem;
	
	/** First miplevel used in the texture being streamed in, which is IntermediateTextureRHI when it is valid. */
	int32	PendingFirstMip;

	/** First mip level used in Texture2DRHI. This is always correct as long as Texture2DRHI is allocated, regardless of streaming status. */
	int32 CurrentFirstMip;

	/** Pending async create texture task, if any.															*/
	TScopedPointer<FAsyncCreateTextureTask> AsyncCreateTextureTask;
	
	/** Local copy/ cache of mip data between creation and first call to InitRHI.							*/
	void*				MipData[MAX_TEXTURE_MIP_COUNT];
	/** Potentially outstanding texture I/O requests.														*/
	uint64				IORequestIndices[MAX_TEXTURE_MIP_COUNT];
	/** Number of file I/O requests for current request														*/
	int32					IORequestCount;

#if WITH_EDITORONLY_DATA
	/** Pending async derived data streaming tasks															*/
	TIndirectArray<FAsyncStreamDerivedMipTask> PendingAsyncStreamDerivedMipTasks;
#endif // #if WITH_EDITORONLY_DATA

	/** 2D texture version of TextureRHI which is used to lock the 2D texture during mip transitions.		*/
	FTexture2DRHIRef	Texture2DRHI;
	/** Intermediate texture used to fulfill mip change requests. Swapped in FinalizeMipCount.				*/
	FTexture2DRHIRef	IntermediateTextureRHI;
	/** Whether the intermediate texture is being created asynchronously.									*/
	uint32			bUsingAsyncCreation:1;
	/** Whether the current stream request is prioritized higher than normal.	*/
	uint32			bPrioritizedIORequest:1;
	/** Whether the last mip streaming request failed.														*/
	uint32			bDerivedDataStreamRequestFailed:1;
	/** Number of times UpdateMipCount has failed to reallocate memory.										*/
	int32					NumFailedReallocs;

#if STATS
	/** Cached texture size for stats.																		*/
	int32					TextureSize;
	/** Cached intermediate texture size for stats.															*/
	int32					IntermediateTextureSize;
	/** The FName of the LODGroup-specific stat																*/
	FName					LODGroupStatName;
#endif

	/**
	 * Writes the data for a single mip-level into a destination buffer.
	 * @param MipIndex	The index of the mip-level to read.
	 * @param Dest		The address of the destination buffer to receive the mip-level's data.
	 * @param DestPitch	Number of bytes per row
	 */
	void GetData( uint32 MipIndex,void* Dest,uint32 DestPitch );

	/**
	 * Called from the rendering thread to perform the work to kick off a change in ResidentMips.
	 */
	void UpdateMipCount();
	/**
	 * Called from the rendering thread to start async I/O to load in new mips.
	 */
	void LoadMipData();
	/**
	 * Called from the rendering thread to unlock textures and make the data visible to the GPU.
	 */
	void UploadMipData();
	/**
	 * Called from the rendering thread to finalize a mip change.
	 */
	void FinalizeMipCount();
	/**
	 * Called from the rendering thread to cancel async operations for request.
	 */
	void CancelUpdate();

	/** Create RHI sampler states. */
	void CreateSamplerStates(float MipMapBias);

	/** Returns the default mip map bias for this texture. */
	int32 GetDefaultMipMapBias() const;

	// releases and recreates sampler state objects.
	// used when updating mip map bias offset
	void RefreshSamplerStates();
};

/** A dynamic 2D texture resource. */
class FTexture2DDynamicResource : public FTextureResource
{
public:
	/** Initialization constructor. */
	FTexture2DDynamicResource(class UTexture2DDynamic* InOwner);

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const;

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const;

	/** Called when the resource is initialized. This is only called by the rendering thread. */
	virtual void InitRHI();

	/** Called when the resource is released. This is only called by the rendering thread. */
	virtual void ReleaseRHI();

	/** Returns the Texture2DRHI, which can be used for locking/unlocking the mips. */
	FTexture2DRHIRef GetTexture2DRHI();

private:
	/** The owner of this resource. */
	class UTexture2DDynamic* Owner;
	/** Texture2D reference, used for locking/unlocking the mips. */
	FTexture2DRHIRef Texture2DRHI;
};

/** Stores information about a mip map, used by FTexture2DArrayResource to mirror game thread data. */
class FMipMapDataEntry
{
public:
	uint32 SizeX;
	uint32 SizeY;
	TArray<uint8> Data;
};

/** Stores information about a single texture in FTexture2DArrayResource. */
class FTextureArrayDataEntry
{
public:

	FTextureArrayDataEntry() : 
		NumRefs(0)
	{}

	/** Number of FTexture2DArrayResource::AddTexture2D calls that specified this texture. */
	int32 NumRefs;

	/** Mip maps of the texture. */
	TArray<FMipMapDataEntry, TInlineAllocator<MAX_TEXTURE_MIP_COUNT> > MipData;
};

/** 
 * Stores information about a UTexture2D so the rendering thread can access it, 
 * Even though the UTexture2D may have changed by the time the rendering thread gets around to it.
 */
class FIncomingTextureArrayDataEntry : public FTextureArrayDataEntry
{
public:

	FIncomingTextureArrayDataEntry() {}

	FIncomingTextureArrayDataEntry(UTexture2D* InTexture);

	int32 SizeX;
	int32 SizeY;
	int32 NumMips;
	TextureGroup LODGroup;
	EPixelFormat Format;
	ESamplerFilter Filter;
	bool bSRGB;
};

/** Represents a 2D Texture Array to the renderer. */
class FTexture2DArrayResource : public FTextureResource
{
public:

	FTexture2DArrayResource() :
		SizeX(0),
		bDirty(false),
		bPreventingReallocation(false)
	{}

	// Rendering thread functions

	/** 
	 * Adds a texture to the texture array.  
	 * This is called on the rendering thread, so it must not dereference NewTexture.
	 */
	void AddTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry);

	/** Removes a texture from the texture array, and potentially removes the CachedData entry if the last ref was removed. */
	void RemoveTexture2D(const UTexture2D* NewTexture);

	/** Updates a CachedData entry (if one exists for this texture), with a new texture. */
	void UpdateTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry);

	/** Initializes the texture array resource if needed, and re-initializes if the texture array has been made dirty since the last init. */
	void UpdateResource();

	/** Returns the index of a given texture in the texture array. */
	int32 GetTextureIndex(const UTexture2D* Texture) const;
	int32 GetNumValidTextures() const;

	/**
	* Called when the resource is initialized. This is only called by the rendering thread.
	*/
	virtual void InitRHI();

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const
	{
		return SizeX;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const
	{
		return SizeY;
	}

	/** Prevents reallocation from removals of the texture array until EndPreventReallocation is called. */
	void BeginPreventReallocation();

	/** Restores the ability to reallocate the texture array. */
	void EndPreventReallocation();

private:

	/** Texture data, has to persist past the first InitRHI call, because more textures may be added later. */
	TMap<const UTexture2D*, FTextureArrayDataEntry> CachedData;
	uint32 SizeX;
	uint32 SizeY;
	uint32 NumMips;
	TextureGroup LODGroup;
	EPixelFormat Format;
	ESamplerFilter Filter;

	bool bSRGB;
	bool bDirty;
	bool bPreventingReallocation;

	/** Copies data from DataEntry into Dest, taking stride into account. */
	void GetData(const FTextureArrayDataEntry& DataEntry, int32 MipIndex, void* Dest, uint32 DestPitch);
};

/**
 * FDeferredUpdateResource for resources that need to be updated after scene rendering has begun
 * (should only be used on the rendering thread)
 */
class FDeferredUpdateResource
{
public:
	/**
	 * Constructor, initializing UpdateListLink.
	 */
	FDeferredUpdateResource()
		:	UpdateListLink(NULL)
		,	bOnlyUpdateOnce(false)
	{}

	/**
	 * Iterate over the global list of resources that need to
	 * be updated and call UpdateResource on each one.
	 */
	ENGINE_API static void UpdateResources();

	/** 
	 * This is reset after all viewports have been rendered
	 */
	static void ResetNeedsUpdate()
	{
		bNeedsUpdate = true;
	}

	// FDeferredUpdateResource interface

	/**
	 * Updates the resource
	 */
	virtual void UpdateResource() = 0;

protected:

	/**
	 * Add this resource to deferred update list
	 * @param OnlyUpdateOnce - flag this resource for a single update if true
	 */
	void AddToDeferredUpdateList( bool OnlyUpdateOnce );

	/**
	 * Remove this resource from deferred update list
	 */
	void RemoveFromDeferredUpdateList();

private:
	/** 
	 * Resources can be added to this list if they need a deferred update during scene rendering.
	 * @return global list of resource that need to be updated. 
	 */
	static TLinkedList<FDeferredUpdateResource*>*& GetUpdateList();
	/** This resource's link in the global list of resources needing clears. */
	TLinkedList<FDeferredUpdateResource*> UpdateListLink;
	/** if true then UpdateResources needs to be called */
	ENGINE_API static bool bNeedsUpdate;
	/** if true then remove this resource from the update list after a single update */
	bool bOnlyUpdateOnce;
};

/**
 * FTextureResource type for render target textures.
 */
class FTextureRenderTargetResource : public FTextureResource, public FRenderTarget, public FDeferredUpdateResource
{
public:
	/**
	 * Constructor, initializing ClearLink.
	 */
	FTextureRenderTargetResource()
	{}

	/** 
	 * Return true if a render target of the given format is allowed
	 * for creation
	 */
	ENGINE_API static bool IsSupportedFormat( EPixelFormat Format );

	// FTextureRenderTargetResource interface
	
	virtual class FTextureRenderTarget2DResource* GetTextureRenderTarget2DResource()
	{
		return NULL;
	}
	virtual void ClampSize(int32 SizeX,int32 SizeY) {}

	// FRenderTarget interface.
	virtual FIntPoint GetSizeXY() const = 0;

	/** 
	 * Render target resource should be sampled in linear color space
	 *
	 * @return display gamma expected for rendering to this render target 
	 */
	virtual float GetDisplayGamma() const;
};

/**
 * FTextureResource type for 2D render target textures.
 */
class FTextureRenderTarget2DResource : public FTextureRenderTargetResource
{
public:
	
	/** 
	 * Constructor
	 * @param InOwner - 2d texture object to create a resource for
	 */
	FTextureRenderTarget2DResource(const class UTextureRenderTarget2D* InOwner);

	FORCEINLINE FLinearColor GetClearColor()
	{
		return ClearColor;
	}

	// FTextureRenderTargetResource interface

	/** 
	 * 2D texture RT resource interface 
	 */
	virtual class FTextureRenderTarget2DResource* GetTextureRenderTarget2DResource()
	{
		return this;
	}

	/**
	 * Clamp size of the render target resource to max values
	 *
	 * @param MaxSizeX max allowed width
	 * @param MaxSizeY max allowed height
	 */
	virtual void ClampSize(int32 SizeX,int32 SizeY);
	
	// FRenderResource interface.

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI();

	// FDeferredClearResource interface

	/**
	 * Clear contents of the render target
	 */
	virtual void UpdateResource();

	// FRenderTarget interface.
	virtual FIntPoint GetSizeXY() const;

	/** 
	 * Render target resource should be sampled in linear color space
	 *
	 * @return display gamma expected for rendering to this render target 
	 */
	virtual float GetDisplayGamma() const;

	/** 
	 * @return TextureRHI for rendering 
	 */
	FTexture2DRHIRef GetTextureRHI() { return Texture2DRHI; }

private:
	/** The UTextureRenderTarget2D which this resource represents. */
	const class UTextureRenderTarget2D* Owner;
	/** Texture resource used for rendering with and resolving to */
	FTexture2DRHIRef Texture2DRHI;
	/** the color the texture is cleared to */
	FLinearColor ClearColor;
	EPixelFormat Format;
	int32 TargetSizeX,TargetSizeY;
};

/**
 * FTextureResource type for cube render target textures.
 */
class FTextureRenderTargetCubeResource : public FTextureRenderTargetResource
{
public:

	/** 
	 * Constructor
	 * @param InOwner - cube texture object to create a resource for
	 */
	FTextureRenderTargetCubeResource(const class UTextureRenderTargetCube* InOwner)
		:	Owner(InOwner)
	{
	}

	/** 
	 * Cube texture RT resource interface 
	 */
	virtual class FTextureRenderTargetCubeResource* GetTextureRenderTargetCubeResource()
	{
		return this;
	}

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI();	

	/**
	 * Clear contents of the render target. Clears each face of the cube
	 * This is only called by the rendering thread.
	 */
	virtual void UpdateResource();

	// FRenderTarget interface.

	/**
	 * @return dimensions of the target
	 */
	virtual FIntPoint GetSizeXY() const;

	/** 
	 * @return TextureRHI for rendering 
	 */
	FTextureCubeRHIRef GetTextureRHI() { return TextureCubeRHI; }

	/** 
	* Render target resource should be sampled in linear color space
	*
	* @return display gamma expected for rendering to this render target 
	*/
	float GetDisplayGamma() const;

	/**
	* Copy the texels of a single face of the cube into an array.
	* @param OutImageData - float16 values will be stored in this array.
	* @param InFlags - read flags. ensure cubeface member has been set.
	* @param InRect - Rectangle of texels to copy.
	* @return true if the read succeeded.
	*/
	ENGINE_API bool ReadPixels(TArray< FColor >& OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect = FIntRect(0, 0, 0, 0));

	/**
	* Copy the texels of a single face of the cube into an array.
	* @param OutImageData - float16 values will be stored in this array.
	* @param InFlags - read flags. ensure cubeface member has been set.
	* @param InRect - Rectangle of texels to copy.
	* @return true if the read succeeded.
	*/
	ENGINE_API bool ReadPixels(TArray<FFloat16Color>& OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect = FIntRect(0, 0, 0, 0));

private:
	/** The UTextureRenderTargetCube which this resource represents. */
	const class UTextureRenderTargetCube* Owner;
	/** Texture resource used for rendering with and resolving to */
	FTextureCubeRHIRef TextureCubeRHI;
	/** Target surfaces for each cube face */
	FTexture2DRHIRef CubeFaceSurfaceRHI;

	/** Represents the current render target (from one of the cube faces)*/
	FTextureCubeRHIRef RenderTargetCubeRHI;

	/** Face currently used for target surface */
	ECubeFace CurrentTargetFace;
};

/**
 * FTextureResource type for movie textures.
 */
class FTextureMovieResource : public FTextureResource, public FRenderTarget, public FDeferredUpdateResource
{
public:

	/** 
	 * Constructor
	 * @param InOwner - movie texture object to create a resource for
	 */
	FTextureMovieResource(const class UTextureMovie* InOwner)
		:	Owner(InOwner)
	{
	}

	// FRenderResource interface.

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI();	

	// FDeferredClearResource interface

	/**
	 * Decodes the next frame of the movie stream and renders the result to this movie texture target
	 */
	virtual void UpdateResource();

	// FRenderTarget interface.
	virtual FIntPoint GetSizeXY() const;

private:
	/** The UTextureRenderTarget2D which this resource represents. */
	const class UTextureMovie* Owner;
	/** Texture resource used for rendering with and resolving to */
	FTexture2DRHIRef Texture2DRHI;
};

/**
 * Structure containing all information related to an LOD group and providing helper functions to calculate
 * the LOD bias of a given group.
 */
struct FTextureLODSettings
{
	/**
	 * Initializes LOD settings by reading them from the passed in filename/ section.
	 *
	 * @param	IniFilename		Filename of ini to read from.
	 * @param	IniSection		Section in ini to look for settings
	 */
	ENGINE_API void Initialize( const FString& IniFilename, const TCHAR* IniSection );

	/**
	 * Initializes LOD settings by reading them from the passed in filename/ section.
	 *
	 * @param	IniFile			Preloaded ini file object to load from
	 * @param	IniSection		Section in ini to look for settings
	 */
	ENGINE_API void Initialize(const FConfigFile& IniFile, const TCHAR* IniSection);

	/**
	 * Calculates and returns the LOD bias based on texture LOD group, LOD bias and maximum size.
	 *
	 * @param	Texture			Texture object to calculate LOD bias for.
	 * @param	bIncTextureBias	If true, takes the texures LOD bias into consideration
	 * @return	LOD bias
	 */
	ENGINE_API int32 CalculateLODBias( UTexture* Texture, bool bIncTextureBias = true ) const;

	/**
	 * Calculates and returns the LOD bias based on the information provided.
	 *
	 * @param	Width						Width of the texture
	 * @param	Height						Height of the texture
	 * @param	LODGroup					Which LOD group the texture belongs to
	 * @param	LODBias						LOD bias to include in the calculation
	 * @param	NumCinematicMipLevels		The texture cinematic mip levels to include in the calculation
	 * @param	MipGenSetting				Mip generation setting
	 * @return	LOD bias
	 */
	ENGINE_API int32 CalculateLODBias( int32 Width, int32 Height, int32 LODGroup, int32 LODBias, int32 NumCinematicMipLevels, TextureMipGenSettings MipGenSetting ) const;

	/** 
	* Useful for stats in the editor.
	*
	* @param LODBias			Default LOD at which the texture renders. Platform dependent, call FTextureLODSettings::CalculateLODBias(Texture)
	*/
	ENGINE_API void ComputeInGameMaxResolution(int32 LODBias, UTexture& Texture, uint32& OutSizeX, uint32& OutSizeY) const;

#if WITH_EDITORONLY_DATA
	void GetMipGenSettings( const UTexture& Texture, TextureMipGenSettings& OutMipGenSettings, float& OutSharpen, uint32& OutKernelSize, bool& bOutDownsampleWithAverage, bool& bOutSharpenWithoutColorShift, bool &bOutBorderColorBlack ) const;
#endif // #if WITH_EDITORONLY_DATA

	/**
	 * Will return the LODBias for a passed in LODGroup
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	LODBias
	 */
	int32 GetTextureLODGroupLODBias( int32 InLODGroup ) const;

	/**
	 * Returns the LODGroup setting for number of streaming mip-levels.
	 * -1 means that all mip-levels are allowed to stream.
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	Number of streaming mip-levels for textures in the specified LODGroup
	 */
	int32 GetNumStreamedMips( int32 InLODGroup ) const;

	/**
	 * Returns the filter state that should be used for the passed in texture, taking
	 * into account other system settings.
	 *
	 * @param	Texture		Texture to retrieve filter state for
	 * @return	Filter sampler state for passed in texture
	 */
	ESamplerFilter GetSamplerFilter( const UTexture* Texture ) const;

	/**
	 * Returns the texture group names, sorted like enum.
	 *
	 * @return array of texture group names
	 */
	ENGINE_API static TArray<FString> GetTextureGroupNames();

	/** LOD settings for a single texture group. */
	struct FTextureLODGroup 
	{
		FTextureLODGroup()
		:	MinLODMipCount(0)
		,	MaxLODMipCount(12)
		,	LODBias(0) 
		,	Filter(SF_AnisotropicPoint)
		,	NumStreamedMips(-1)
		,	MipGenSettings(TMGS_SimpleAverage)
		{}
		/** Minimum LOD mip count below which the code won't bias.						*/
		int32 MinLODMipCount;
		/** Maximum LOD mip count. Bias will be adjusted so texture won't go above.		*/
		int32 MaxLODMipCount;
		/** Group LOD bias.																*/
		int32 LODBias;
		/** Sampler filter state.														*/
		ESamplerFilter Filter;
		/** Number of mip-levels that can be streamed. -1 means all mips can stream.	*/
		int32 NumStreamedMips;
		/** Defines how the the mip-map generation works, e.g. sharpening				*/
		TextureMipGenSettings MipGenSettings;
	};

protected:
	/**
	 * Reads a single entry and parses it into the group array.
	 *
	 * @param	GroupId			Id/ enum of group to parse
	 * @param	GroupName		Name of group to look for in ini
	 * @param	IniFile			Config file to read from.
	 * @param	IniSection		Section in ini to look for settings
	 */
	void ReadEntry( int32 GroupId, const TCHAR* GroupName, const FConfigFile& IniFile, const TCHAR* IniSection );

	/**
	 * TextureLODGroups access with bounds check
	 *
	 * @param   GroupIndex      usually from Texture.LODGroup
	 * @return                  A handle to the indexed LOD group. 
	 */
	const FTextureLODGroup& GetTextureLODGroup(TextureGroup GroupIndex) const;

	/** Array of LOD settings with entries per group. */
	FTextureLODGroup TextureLODGroups[TEXTUREGROUP_MAX];
};







#define TEXTURE_H_INCLUDED 1 // needed by TargetPlatform, so TTargetPlatformBase template knows if it can use UTexture in GetDefaultTextureFormatName, or rather just declare it
