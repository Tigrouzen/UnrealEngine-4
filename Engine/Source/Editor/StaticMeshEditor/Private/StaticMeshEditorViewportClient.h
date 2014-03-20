// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"

/** Viewport Client for the preview viewport */
class FStaticMeshEditorViewportClient : public FEditorViewportClient, public TSharedFromThis<FStaticMeshEditorViewportClient>
{
public:
	FStaticMeshEditorViewportClient(TWeakPtr<IStaticMeshEditor> InStaticMeshEditor, FPreviewScene& InPreviewScene, UStaticMesh* InPreviewStaticMesh, UStaticMeshComponent* InPreviewStaticMeshComponent);

	// FEditorViewportClient interface
	virtual void MouseMove(FViewport* Viewport,int32 x, int32 y) OVERRIDE;
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad=false) OVERRIDE;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples=1, bool bGamepad=false) OVERRIDE;
	virtual void ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) OVERRIDE;
	virtual void Tick(float DeltaSeconds) OVERRIDE;
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) OVERRIDE;
	virtual void DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas ) OVERRIDE;
	virtual bool InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale ) OVERRIDE;
	virtual void TrackingStarted( const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge ) OVERRIDE;
	virtual void TrackingStopped() OVERRIDE;
	virtual FWidget::EWidgetMode GetWidgetMode() const OVERRIDE;
	virtual FVector GetWidgetLocation() const;
	virtual FMatrix GetWidgetCoordSystem() const;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const { return COORD_Local; }
	virtual bool ShouldOrbitCamera() const OVERRIDE;
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily) OVERRIDE;

	/** 
	 *	Updates the static mesh and static mesh component being used in the Static Mesh Editor.
	 *
	 *	@param	InStaticMesh				The static mesh handled by the editor.
	 *	@param	InStaticMeshComponent		The static mesh component component from the viewport's scene.
	 */
	void SetPreviewMesh(UStaticMesh* InStaticMesh, UStaticMeshComponent* InPreviewStaticMeshComponent);

	/** Retrieves the selected edge set. */
	TSet< int32 >& GetSelectedEdges();

	/**
	 * Called when the selected socket changes
	 *
	 * @param SelectedSocket	The newly selected socket or NULL if no socket is selected
	 */
	void OnSocketSelectionChanged( UStaticMeshSocket* SelectedSocket );

	void ResetCamera();

	/**
	 *	Draws the UV overlay for the current LOD.
	 *
	 *	@param	InViewport					The viewport to draw to
	 *	@param	InCanvas					The canvas to draw to
	 *	@param	InTextYPos					The Y-position to begin drawing the UV-Overlay at (due to text displayed above it).
	 */
	void DrawUVsForMesh(FViewport* InViewport, FCanvas* InCanvas, int32 InTextYPos );

	/** Callback for toggling the UV overlay show flag. */
	void SetDrawUVOverlay();
	
	/** Callback for checking the UV overlay show flag. */
	bool IsSetDrawUVOverlayChecked() const;


	/** Callback for toggling the normals show flag. */
	void SetShowNormals();
	
	/** Callback for checking the normals show flag. */
	bool IsSetShowNormalsChecked() const;

	/** Callback for toggling the tangents show flag. */
	void SetShowTangents();
	
	/** Callback for checking the tangents show flag. */
	bool IsSetShowTangentsChecked() const;

	/** Callback for toggling the binormals show flag. */
	void SetShowBinormals();
	
	/** Callback for checking the binormals show flag. */
	bool IsSetShowBinormalsChecked() const;

	/** Callback for toggling the socket show flag. */
	void SetShowSockets();

	/** Callback for checking the socket show flag. */
	bool IsSetShowSocketsChecked() const;

	/** Callback for toggling the pivot show flag. */
	void SetShowPivot();

	/** Callback for checking the pivot show flag. */
	bool IsSetShowPivotChecked() const;

	/** Callback for toggling the additional data drawing flag. */
	void SetDrawAdditionalData();

	/** Callback for checking the additional data drawing flag. */
	bool IsSetDrawAdditionalData() const;

private:
	/** The Simplygon logo to be drawn when Simplygon has been used on the static mesh. */
	UTexture2D* SimplygonLogo;

	/** Component for the static mesh. */
	UStaticMeshComponent* StaticMeshComponent;

	/** The static mesh being used in the editor. */
	UStaticMesh* StaticMesh;

	/** Pointer back to the StaticMesh editor tool that owns us */
	TWeakPtr<IStaticMeshEditor> StaticMeshEditorPtr;

	/** Flags for various options in the editor. */
	bool bDrawUVs;
	bool bShowSockets;
	bool bDrawNormals;
	bool bDrawTangents;
	bool bDrawBinormals;
	bool bShowPivot;
	bool bDrawAdditionalData;

	/** true when the user is manipulating a socket widget. */
	bool bManipulating;

	FWidget::EWidgetMode WidgetMode;

	/** The current widget axis the mouse is highlighting. */
	EAxis::Type SocketManipulateAxis;

	/** Holds the currently selected edges. */
	typedef TSet< int32 > FSelectedEdgeSet;
	FSelectedEdgeSet SelectedEdgeIndices;

	/** Cached vertex positions for the currently selected edges. Used for rendering */
	TArray<FVector> SelectedEdgeVertices;

	/** Cached tex coords for the currently selected edges. Used for rendering UV coordinates*/
	TArray<FVector2D> SelectedEdgeTexCoords[MAX_STATIC_TEXCOORDS];
};