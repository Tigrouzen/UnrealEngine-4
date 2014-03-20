// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"

/**
 * An editor viewport client subclass for the SCS editor viewport.
 */
class FSCSEditorViewportClient : public FEditorViewportClient, public TSharedFromThis<FSCSEditorViewportClient>
{
public:
	/**
	 * Constructor.
	 *
	 * @param InBlueprintEditorPtr A weak reference to the Blueprint Editor context.
	 * @param InPreviewScene The preview scene to use.
	 */
	FSCSEditorViewportClient(TWeakPtr<class FBlueprintEditor>& InBlueprintEditorPtr, FPreviewScene& InPreviewScene);

	/**
	 * Destructor.
	 */
	virtual ~FSCSEditorViewportClient();

	// FEditorViewportClient interface
	virtual void Tick(float DeltaSeconds) OVERRIDE;
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily) OVERRIDE;
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) OVERRIDE;
	virtual void DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas ) OVERRIDE;
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad=false) OVERRIDE;
	virtual void ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) OVERRIDE;
	virtual bool InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale ) OVERRIDE;
	virtual void TrackingStarted( const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge ) OVERRIDE;
	virtual void TrackingStopped() OVERRIDE;
	virtual FWidget::EWidgetMode GetWidgetMode() const OVERRIDE;
	virtual void SetWidgetMode( FWidget::EWidgetMode NewMode ) OVERRIDE;
	virtual void SetWidgetCoordSystemSpace( ECoordSystem NewCoordSystem ) OVERRIDE;
	virtual FVector GetWidgetLocation() const;
	virtual FMatrix GetWidgetCoordSystem() const;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const { return WidgetCoordSystem; }


	/** 
	 * Recreates the preview scene and invalidates the owning viewport.
	 *
	 * @param bResetCamera Whether or not to reset the camera after recreating the preview scene.
	 */
	void InvalidatePreview(bool bResetCamera = true);

	/**
     * Resets the camera position
	 */
	void ResetCamera();

	/**
     * Determines whether or not realtime preview is enabled.
	 * 
	 * @return true if realtime preview is enabled, false otherwise.
	 */
	bool GetRealtimePreview() const
	{
		return IsRealtime();
	}

	/**
     * Toggles realtime preview on/off.
	 */
	void ToggleRealtimePreview();

	/**
     * Determines whether or not the preview scene is valid.
	 * 
	 * @return true if the preview scene is valid, false otherwise.
	 */
	bool IsPreviewSceneValid() const;

	/**
	 * Focuses the viewport on the selected components
	 */
	void FocusViewportToSelection();

	/**
	 * Returns true if simulate is enabled in the viewport
	 */
	bool GetIsSimulateEnabled();

	/**
	 * Will toggle the simulation mode of the viewport
	 */
	void ToggleIsSimulateEnabled();

	/**
	 * Returns true if the floor is currently visible in the viewport
	 */
	bool GetShowFloor();

	/**
	 * Will toggle the floor's visibility in the viewport
	 */
	void ToggleShowFloor();

	/**
	 * Returns true if the grid is currently visible in the viewport
	 */
	bool GetShowGrid();

	/**
	 * Will toggle the grid's visibility in the viewport
	 */
	void ToggleShowGrid();

	/**
	 * Gets the current preview actor instance.
	 */
	AActor* GetPreviewActor() const;

protected:
	/**
	 * Initiates a transaction.
	 */
	void BeginTransaction(const FText& Description);

	/**
	 * Ends the current transaction, if one exists.
	 */
	void EndTransaction();

	/**
	 * Creates/updates the preview actor for the given blueprint.
	 *
	 * @param InBlueprint			The Blueprint to create or update the preview for.
	 * @param bInForceFullUpdate	Force a full update to respawn actors.
	 */
	void UpdatePreviewActorForBlueprint(UBlueprint* InBlueprint, bool bInForceFullUpdate = false);

	/**
	 * Destroy the Blueprint preview.
	 */
	void DestroyPreview();

	/**
	 * Updates preview bounds and floor positioning
	 */
	void RefreshPreviewBounds();

private:
	FWidget::EWidgetMode WidgetMode;
	ECoordSystem WidgetCoordSystem;

	/** Weak reference to the editor hosting the viewport */
	TWeakPtr<class FBlueprintEditor> BlueprintEditorPtr;

	/** The Blueprint associated with the current preview */
	UBlueprint* PreviewBlueprint;

	/** The preview actor representing the current preview */
	mutable TWeakObjectPtr<AActor> PreviewActorPtr;

	/** The full bounds of the preview scene (encompasses all visible components) */
	FBoxSphereBounds PreviewActorBounds;

	/** If true then we are manipulating a specific property or component */
	bool bIsManipulating;

	/** The current transaction for undo/redo */
	FScopedTransaction* ScopedTransaction;

	/** Floor static mesh component */
	UStaticMeshComponent* EditorFloorComp;

	/** If true, the physics simulation gets ticked */
	bool bIsSimulateEnabled;
};