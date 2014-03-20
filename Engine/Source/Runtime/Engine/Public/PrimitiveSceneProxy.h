// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrimitiveSceneProxy.h: Primitive scene proxy definition.
=============================================================================*/

#pragma once

#include "SceneManagement.h"
#include "UniformBuffer.h"

/** Data for a simple dynamic light. */
class FSimpleLightEntry
{
public:
	FVector4 PositionAndRadius;
	FVector Color;
	float Exponent;
	bool bAffectTranslucency;
};

namespace EDrawDynamicFlags
{
	enum Type
	{
		ForceLowestLOD = 0x1
	};
}

/**
 * Encapsulates the data which is mirrored to render a UPrimitiveComponent parallel to the game thread.
 * This is intended to be subclassed to support different primitive types.  
 */
class FPrimitiveSceneProxy
{
public:

	/** Initialization constructor. */
	ENGINE_API FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, FName ResourceName = NAME_None);

	/** Virtual destructor. */
	ENGINE_API virtual ~FPrimitiveSceneProxy();

	/**
	 * Updates selection for the primitive proxy. This simply sends a message to the rendering thread to call SetSelection_RenderThread.
	 * This is called in the game thread as selection is toggled.
	 * @param bInSelected - true if the parent actor is selected in the editor
	 */
	void SetSelection_GameThread(const bool bInSelected);

	/**
     * Updates hover state for the primitive proxy. This simply sends a message to the rendering thread to call SetHovered_RenderThread.
     * This is called in the game thread as hover state changes
     * @param bInHovered - true if the parent actor is hovered
     */
	void SetHovered_GameThread(const bool bInHovered);

	/**
	 * Updates the hidden editor view visibility map on the game thread which just enqueues a command on the render thread
	 */
	void SetHiddenEdViews_GameThread( uint64 InHiddenEditorViews );

	/** @return True if the primitive is visible in the given View. */
	ENGINE_API bool IsShown(const FSceneView* View) const;

	/** @return True if the primitive is casting a shadow. */
	ENGINE_API bool IsShadowCast(const FSceneView* View) const;

	/** Helper for components that want to render bounds. */
	ENGINE_API void RenderBounds(FPrimitiveDrawInterface* PDI, const FEngineShowFlags& EngineShowFlags, const FBoxSphereBounds& Bounds, bool bRenderInEditor) const;

	/** Returns the LOD that the primitive will render at for this view. */
	virtual int32 GetLOD(const FSceneView* View) const { return INDEX_NONE; }
	
	/**
	 * Creates the hit proxies are used when DrawDynamicElements is called.
	 * Called in the game thread.
	 * @param OutHitProxies - Hit proxes which are created should be added to this array.
	 * @return The hit proxy to use by default for elements drawn by DrawDynamicElements.
	 */
	ENGINE_API virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies);

	/**
	 * Draws the primitive's static elements.  This is called from the rendering thread once when the scene proxy is created.
	 * The static elements will only be rendered if GetViewRelevance declares static relevance.
	 * @param PDI - The interface which receives the primitive elements.
	 */
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) {}

	/**
	 * Draws the primitive's dynamic elements.  This is called from the rendering thread for each frame of each view.
	 * The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
	 * Called in the rendering thread.
	 * @param PDI - The interface which receives the primitive elements.
	 * @param View - The view which is being rendered.
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View) {}

	/**
	 * Draws the primitive's dynamic elements.  This is called from the rendering thread for each frame of each view.
	 * The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
	 * Called in the rendering thread.
	 * @param PDI - The interface which receives the primitive elements.
	 * @param View - The view which is being rendered.
	 * @param Flags - Optional flags
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View, uint32 DrawDynamicFlags ) { DrawDynamicElements( PDI, View ); }

	/**
	 * Determines the relevance of this primitive's elements to the given view.
	 * Called in the rendering thread.
	 * @param View - The view to determine relevance for.
	 * @return The relevance of the primitive's elements to the view.
	 */
	ENGINE_API virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	/**
	 *	Called during InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(const FSceneViewFamily* ViewFamily, const uint32 VisibilityMap, int32 FrameNumber) {}

	/** Callback from the renderer to gather simple lights that this proxy wants renderered. */
	virtual void GatherSimpleLights(TArray<FSimpleLightEntry, SceneRenderingAllocator>& OutSimpleLights) const {}

	/**
	 *	Determines the relevance of this primitive's elements to the given light.
	 *	@param	LightSceneProxy			The light to determine relevance for
	 *	@param	bDynamic (output)		The light is dynamic for this primitive
	 *	@param	bRelevant (output)		The light is relevant for this primitive
	 *	@param	bLightMapped (output)	The light is light mapped for this primitive
	 */
	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const
	{
		// Determine the lights relevance to the primitive.
		bDynamic = true;
		bRelevant = true;
		bLightMapped = false;
		bShadowMapped = false;
	}

	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void CreateRenderThreadResources() {}

	/**
	 * Called by the rendering thread to notify the proxy when a light is no longer
	 * associated with the proxy, so that it can clean up any cached resources.
	 * @param Light - The light to be removed.
	 */
	virtual void OnDetachLight(const FLightSceneInfo* Light)
	{
	}

	/**
	 * Called to notify the proxy when its transform has been updated.
	 * Called in the thread that owns the proxy; game or rendering.
	 */
	virtual void OnTransformChanged()
	{
	}

	/**
	 * Called to notify the proxy when its actor position has been updated.
	 * Called in the thread that owns the proxy; game or rendering.
	 */
	virtual void OnActorPositionChanged() {}

	/**
	* @return true if the proxy can be culled when occluded by other primitives
	*/
	virtual bool CanBeOccluded() const
	{
		return true;
	}

	virtual bool ShowInBSPSplitViewmode() const
	{
		return false;
	}

	/**
	 * Determines the DPG to render the primitive in regardless of view.
	 * Should only be called if HasViewDependentDPG()==true.
	 */
	virtual uint8 GetStaticDepthPriorityGroup() const
	{
		check(!HasViewDependentDPG());
		return StaticDepthPriorityGroup;
	}

	/**
	 * Determines the DPG to render the primitive in for the given view.
	 * May be called regardless of the result of HasViewDependentDPG.
	 * @param View - The view to determine the primitive's DPG for.
	 * @return The DPG the primitive should be rendered in for the given view.
	 */
	uint8 GetDepthPriorityGroup(const FSceneView* View) const
	{
		return (bUseViewOwnerDepthPriorityGroup && IsOwnedBy(View->ViewActor)) ?
			ViewOwnerDepthPriorityGroup :
			StaticDepthPriorityGroup;
	}

	/** Every derived class should override these functions */
	virtual uint32 GetMemoryFootprint( void ) const = 0;
	uint32 GetAllocatedSize( void ) const { return( Owners.GetAllocatedSize() ); }

	/**
	 * Set the collision flag on the scene proxy to enable/disable collision drawing
	 *
	 * @param const bool bNewEnabled new state for collision drawing
	 */
	void SetCollisionEnabled_GameThread(const bool bNewEnabled);

	/**
	 * Set the collision flag on the scene proxy to enable/disable collision drawing (RENDER THREAD)
	 *
	 * @param const bool bNewEnabled new state for collision drawing
	 */
	void SetCollisionEnabled_RenderThread(const bool bNewEnabled);

	// Accessors.
	inline FSceneInterface* GetScene() const { return Scene; }
	inline FPrimitiveComponentId GetPrimitiveComponentId() const { return PrimitiveComponentId; }
	inline FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const { return PrimitiveSceneInfo; }
	inline const FMatrix& GetLocalToWorld() const { return LocalToWorld; }
	inline bool IsLocalToWorldDeterminantNegative() const { return bIsLocalToWorldDeterminantNegative; }
	inline const FBoxSphereBounds& GetBounds() const { return Bounds; }
	inline const FBoxSphereBounds& GetLocalBounds() const { return LocalBounds; }
	inline const FVector2D& GetLightMapResolutionScale() const { return LightMapResolutionScale; }
	inline bool IsLightMapResolutionPadded() const { return bLightMapResolutionPadded; }
	inline ELightMapInteractionType GetLightMapType() const { return (ELightMapInteractionType)LightMapType; }
	inline void SetLightMapResolutionScale(const FVector2D& InLightMapResolutionScale) { LightMapResolutionScale = InLightMapResolutionScale; }
	inline void SetIsLightMapResolutionPadded(bool bInLightMapResolutionPadded) { bLightMapResolutionPadded = bInLightMapResolutionPadded; }
	inline void SetLightMapType(ELightMapInteractionType InLightMapType) { LightMapType = InLightMapType; }
	inline FName GetOwnerName() const { return OwnerName; }
	inline FName GetResourceName() const { return ResourceName; }
	inline FName GetLevelName() const { return LevelName; }
	FORCEINLINE TStatId GetStatId() const 
	{ 
		return StatId; 
	}	
	inline float GetMinDrawDistance() const { return MinDrawDistance; }
	inline float GetMaxDrawDistance() const { return MaxDrawDistance; }
	inline int32 GetVisibilityId() const { return VisibilityId; }
	inline int16 GetTranslucencySortPriority() const { return TranslucencySortPriority; }
	inline bool HasMotionBlurVelocityMeshes() const { return bHasMotionBlurVelocityMeshes; }
	inline bool IsMovable() const { return !IsStatic(); }
	inline bool IsOftenMoving() const { return bOftenMoving; }
	inline bool IsStatic() const { return bStatic; }
	inline bool IsSelectable() const { return bSelectable; }
	inline bool IsSelected() const { return bSelected; }
	inline bool ShouldRenderCustomDepth() const { return bRenderCustomDepth; }
	inline bool ShouldRenderInMainPass() const { return bRenderInMainPass; }
	inline bool IsCollisionEnabled() const { return bCollisionEnabled; }
	inline bool IsHovered() const { return bHovered; }
	inline bool IsOwnedBy(const AActor* Actor) const { return Owners.Find(Actor) != INDEX_NONE; }
	inline bool HasViewDependentDPG() const { return bUseViewOwnerDepthPriorityGroup; }
	inline bool HasStaticLighting() const { return bStaticLighting; }
	inline bool NeedsUnbuiltPreviewLighting() const { return bNeedsUnbuiltPreviewLighting; }
	inline bool CastsStaticShadow() const { return bCastStaticShadow; }
	inline bool CastsDynamicShadow() const { return bCastDynamicShadow; }
	inline bool AffectsDynamicIndirectLighting() const { return bAffectDynamicIndirectLighting; }
	inline float GetLpvBiasMultiplier() const { return LpvBiasMultiplier; }
	inline bool CastsVolumetricTranslucentShadow() const { return bCastVolumetricTranslucentShadow; }
	inline bool CastsHiddenShadow() const { return bCastHiddenShadow; }
	inline bool CastsShadowAsTwoSided() const { return bCastShadowAsTwoSided; }
	inline bool CastsInsetShadow() const { return bCastInsetShadow; }
	inline bool LightAttachmentsAsGroup() const { return bLightAttachmentsAsGroup; }
	inline bool StaticElementsAlwaysUseProxyPrimitiveUniformBuffer() const { return bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer; }
	inline bool ShouldUseAsOccluder() const { return bUseAsOccluder; }
	inline bool AllowApproximateOcclusion() const { return bAllowApproximateOcclusion; }
	inline const TUniformBuffer<FPrimitiveUniformShaderParameters>& GetUniformBuffer() const { return UniformBuffer; }
	inline bool HasPerInstanceHitProxies () const { return bHasPerInstanceHitProxies; }
	inline bool UseEditorCompositing(const FSceneView* View) const { return GIsEditor && bUseEditorCompositing && !View->bIsGameView; }
	inline const FVector& GetActorPosition() const { return ActorPosition; }
	inline const bool ReceivesDecals() const { return bReceivesDecals; }
	inline bool WillEverBeLit() const { return bWillEverBeLit; }
	inline bool HasValidSettingsForStaticLighting() const { return bHasValidSettingsForStaticLighting; }
	inline bool AlwaysHasVelocity() const { return bAlwaysHasVelocity; }
	inline bool TreatAsBackgroundForOcclusion() const { return bTreatAsBackgroundForOcclusion; }
#if WITH_EDITOR
	inline int32 GetNumUncachedStaticLightingInteractions() { return NumUncachedStaticLightingInteractions; }
#endif

	/**
	 *	Returns whether the proxy utilizes custom occlusion bounds or not
	 *
	 *	@return	bool		true if custom occlusion bounds are used, false if not;
	 */
	virtual bool HasCustomOcclusionBounds() const
	{
		return false;
	}

	/**
	 *	Return the custom occlusion bounds for this scene proxy.
	 *	
	 *	@return	FBoxSphereBounds		The custom occlusion bounds.
	 */
	virtual FBoxSphereBounds GetCustomOcclusionBounds() const
	{
		checkf(false, TEXT("GetCustomOcclusionBounds> Should not be called on base scene proxy!"));
		return GetBounds();
	}

	/** 
	 * Drawing helper. Draws nice bouncy line.
	 */
	static ENGINE_API void DrawArc(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, const float Height, const uint32 Segments, const FLinearColor& Color
		, uint8 DepthPriorityGroup,	const float Thickness = 0.0f, const bool bScreenSpace = false);
	
	static ENGINE_API void DrawArrowHead(FPrimitiveDrawInterface* PDI, const FVector& Tip, const FVector& Origin, const float Size, const FLinearColor& Color
		, uint8 DepthPriorityGroup,	const float Thickness = 0.0f, const bool bScreenSpace = false);


	/**
	 * Shifts primitive position and all relevant data by an arbitrary delta.
	 * Called on world origin changes
	 * @param InOffset - The delta to shift by
	 */
	ENGINE_API virtual void ApplyWorldOffset(FVector InOffset);


protected:

	/** Allow subclasses to override the primitive name. Used primarily by BSP. */
	void OverrideOwnerName(FName InOwnerName)
	{
		OwnerName = InOwnerName;
	}

private:
	friend class FScene;

	/** The LightMap method used by the primitive */
	uint32 LightMapType : LMIT_NumBits;

	uint32 bIsLocalToWorldDeterminantNegative : 1;
	uint32 DrawInGame : 1;
	uint32 DrawInEditor : 1;
	uint32 bReceivesDecals : 1;
	uint32 bOnlyOwnerSee : 1;
	uint32 bOwnerNoSee : 1;
	uint32 bStatic : 1;
	uint32 bOftenMoving : 1;
	uint32 bSelected : 1;
	
	/** true if the mouse is currently hovered over this primitive in a level viewport */
	uint32 bHovered : 1;

	/** true if the LightMapResolutionScale value has been padded. */
	uint32 bLightMapResolutionPadded : 1;

	/** true if ViewOwnerDepthPriorityGroup should be used. */
	uint32 bUseViewOwnerDepthPriorityGroup : 1;

	/** true if the primitive has motion blur velocity meshes */
	uint32 bHasMotionBlurVelocityMeshes : 1;

	/** DPG this prim belongs to. */
	uint32 StaticDepthPriorityGroup : SDPG_NumBits;

	/** DPG this primitive is rendered in when viewed by its owner. */
	uint32 ViewOwnerDepthPriorityGroup : SDPG_NumBits;

	/** True if the primitive will cache static lighting. */
	uint32 bStaticLighting : 1;

	/** This primitive has bRenderCustomDepth enabled */
	uint32 bRenderCustomDepth : 1;

	/** If true this primitive Renders in the mainPass */
	uint32 bRenderInMainPass : 1;

	/** If true this primitive will render only after owning level becomes visible */
	uint32 bRequiresVisibleLevelToRender : 1;

	/** Whether component level is currently visible */
	uint32 bIsComponentLevelVisible : 1;
	
	/** Whether this component has any collision enabled */
	uint32 bCollisionEnabled : 1;

	/** Whether the primitive should be treated as part of the background for occlusion purposes. */
	uint32 bTreatAsBackgroundForOcclusion : 1;

protected:

	/** Whether the primitive should be statically lit but has unbuilt lighting, and a preview should be used. */
	uint32 bNeedsUnbuiltPreviewLighting : 1;

	/** True if the primitive wants to use static lighting, but has invalid content settings to do so. */
	uint32 bHasValidSettingsForStaticLighting : 1;

	/** Can be set to false to skip some work only needed on lit primitives. */
	uint32 bWillEverBeLit : 1;

	/** True if the primitive casts dynamic shadows. */
	uint32 bCastDynamicShadow : 1;

	/** True if the primitive casts Reflective Shadow Map shadows (meaning it affects Light Propagation Volumes). */
	uint32 bAffectDynamicIndirectLighting : 1;

	/** True if the primitive casts static shadows. */
	uint32 bCastStaticShadow : 1;

	/** 
	 * Whether the object should cast a volumetric translucent shadow.
	 * Volumetric translucent shadows are useful for primitives with smoothly changing opacity like particles representing a volume, 
	 * But have artifacts when used on highly opaque surfaces.
	 */
	uint32 bCastVolumetricTranslucentShadow : 1;

	/** True if the primitive casts shadows even when hidden. */
	uint32 bCastHiddenShadow : 1;

	/** Whether this primitive should cast dynamic shadows as if it were a two sided material. */
	uint32 bCastShadowAsTwoSided : 1;

	/** Whether this component should create a per-object shadow that gives higher effective shadow resolution.  */
	uint32 bCastInsetShadow : 1;

	/** 
	 * Whether to light this component and any attachments as a group.  This only has effect on the root component of an attachment tree.
	 * When enabled, attached component shadowing settings like bCastInsetShadow, bCastVolumetricTranslucentShadow, etc, will be ignored.
	 * This is useful for improving performance when multiple movable components are attached together.
	 */
	uint32 bLightAttachmentsAsGroup : 1;

	/** 
	 * Whether this proxy always uses UniformBuffer and no other uniform buffers.  
	 * When true, a fast path for updating can be used that does not update static draw lists.
	 */
	uint32 bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer : 1;

	/** Whether the primitive should always be considered to have velocities, even if it hasn't moved. */
	uint32 bAlwaysHasVelocity : 1;

private:

	/** If this is True, this primitive will be used to occlusion cull other primitives. */
	uint32 bUseAsOccluder:1;

	/** If this is True, this primitive doesn't need exact occlusion info. */
	uint32 bAllowApproximateOcclusion : 1;

	/** If this is True, this primitive can be selected in the editor. */
	uint32 bSelectable : 1;

	/** Determines whether or not we allow shadowing fading.  Some objects (especially in cinematics) having the shadow fade/pop out looks really bad. **/
	uint32 bAllowShadowFade : 1;

	/** If this primitive has per-instance hit proxies. */
	uint32 bHasPerInstanceHitProxies : 1;

	/** Whether this primitive should be composited onto the scene after post processing (editor only) */
	uint32 bUseEditorCompositing : 1;

protected:
	/** The bias applied to LPV injection */
	float LpvBiasMultiplier;

private:
	/** The primitive's local to world transform. */
	FMatrix LocalToWorld;

	/** The primitive's bounds. */
	FBoxSphereBounds Bounds;

	/** The primitive's local space bounds. */
	FBoxSphereBounds LocalBounds;

	/** The component's actor's position. */
	FVector ActorPosition;

	/** The hierarchy of owners of this primitive.  These must not be dereferenced on the rendering thread, but the pointer values can be used for identification.  */
	TArray<const AActor*> Owners;

	/** The scene the primitive is in. */
	FSceneInterface* Scene;

	/** 
	 * Id for the component this proxy belongs to.  
	 * This will stay the same for the lifetime of the component, so it can be used to identify the component across re-registers.
	 */
	FPrimitiveComponentId PrimitiveComponentId;

	/** Pointer back to the PrimitiveSceneInfo that owns this Proxy. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** The name of the actor this component is attached to. */
	FName OwnerName;

	/** The name of the resource used by the component. */
	FName ResourceName;

	/** The name of the level the primitive is in. */
	FName LevelName;

	/** The StaticLighting resolution for this mesh */
	FVector2D LightMapResolutionScale;

#if WITH_EDITOR
	/** A copy of the actor's group membership for handling per-view group hiding */
	uint64 HiddenEditorViews;
#endif

	/** The translucency sort priority */
	int16 TranslucencySortPriority;

	/** Used for precomputed visibility */
	int32 VisibilityId;

	/** Used for dynamic stats */
	TStatId StatId;

	/** The primitive's cull distance. */
	float MaxDrawDistance;

	/** The primitive's minimum cull distance. */
	float MinDrawDistance;

	/** The primitive's uniform buffer. */
	TUniformBuffer<FPrimitiveUniformShaderParameters> UniformBuffer;

	/** 
	 * The UPrimitiveComponent this proxy is for, useful for quickly inspecting properties on the corresponding component while debugging.
	 * This should not be dereferenced on the rendering thread.  The game thread can be modifying UObject members at any time.
	 * Use PrimitiveComponentId instead when a component identifier is needed.
	 */
	const UPrimitiveComponent* ComponentForDebuggingOnly;

#if WITH_EDITOR
	/**
	*	How many invalid lights for this primitive, just refer for scene outliner
	*/
	int32 NumUncachedStaticLightingInteractions;
	friend class FLightPrimitiveInteraction;
#endif

	/** Updates the proxy's actor position, called from the game thread. */
	ENGINE_API void UpdateActorPosition(FVector ActorPosition);

	/**
	 * Updates the primitive proxy's cached transforms, and calls OnUpdateTransform to notify it of the change.
	 * Called in the thread that owns the proxy; game or rendering.
	 * @param InLocalToWorld - The new local to world transform of the primitive.
	 * @param InBounds - The new bounds of the primitive.
	 * @param InLocalBounds - The local space bounds of the primitive.
	 */
	ENGINE_API void SetTransform(const FMatrix& InLocalToWorld, const FBoxSphereBounds& InBounds, const FBoxSphereBounds& InLocalBounds, FVector ActorPosition);

	/** Updates the hidden editor view visibility map on the render thread */
	void SetHiddenEdViews_RenderThread( uint64 InHiddenEditorViews );

protected:
	/** Updates selection for the primitive proxy. This is called in the rendering thread by SetSelection_GameThread. */
	void SetSelection_RenderThread(const bool bInSelected);

	/** Updates hover state for the primitive proxy. This is called in the rendering thread by SetHovered_GameThread. */
	void SetHovered_RenderThread(const bool bInHovered);
};
