// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ETileMapEditorTool
{
	enum Type
	{
		Paintbrush,
		Eraser,
		PaintBucket
	};
}

namespace ETileMapLayerPaintingMode
{
	enum Type
	{
		VisualLayers,
		CollisionLayers
	};
}

//////////////////////////////////////////////////////////////////////////
// FEdModeTileMap

class FEdModeTileMap : public FEdMode
{
public:
	static const FEditorModeID EM_TileMap;

public:
	FEdModeTileMap();
	virtual ~FEdModeTileMap();

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) OVERRIDE;
	// End of FSerializableObject

	// FEdMode interface
	virtual bool UsesToolkits() const OVERRIDE;
	virtual void Enter() OVERRIDE;
	virtual void Exit() OVERRIDE;
	//virtual bool BoxSelect(FBox& InBox, bool bInSelect) OVERRIDE;

	//	virtual void PostUndo() OVERRIDE;
	virtual bool MouseMove(FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) OVERRIDE;
	virtual bool CapturedMouseMove(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY) OVERRIDE;
	virtual bool StartTracking(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport) OVERRIDE;
	virtual bool EndTracking(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport) OVERRIDE;
	// 	virtual void Tick(FLevelEditorViewportClient* ViewportClient,float DeltaTime) OVERRIDE;
	virtual bool InputKey(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) OVERRIDE;
	virtual bool InputDelta(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) OVERRIDE;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) OVERRIDE;
	virtual void DrawHUD(FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) OVERRIDE;
	// 	virtual bool Select(AActor* InActor, bool bInSelected) OVERRIDE;
	// 	virtual bool IsSelectionAllowed(AActor* InActor) const OVERRIDE;
	// 	virtual void ActorSelectionChangeNotify() OVERRIDE;
	// 	virtual FVector GetWidgetLocation() const OVERRIDE;
	virtual bool AllowWidgetMove();
	virtual bool ShouldDrawWidget() const OVERRIDE;
	virtual bool UsesTransformWidget() const OVERRIDE;
	virtual void PeekAtSelectionChangedEvent(UObject* ItemUndergoingChange) OVERRIDE;
	// 	virtual int32 GetWidgetAxisToDraw(FWidget::EWidgetMode InWidgetMode) const OVERRIDE;
	// 	virtual bool DisallowMouseDeltaTracking() const OVERRIDE;
	// End of FEdMode interface

	void SetActiveTool(ETileMapEditorTool::Type NewTool);
	ETileMapEditorTool::Type GetActiveTool() const;

	void SetActiveLayerPaintingMode(ETileMapLayerPaintingMode::Type NewMode);
	ETileMapLayerPaintingMode::Type GetActiveLayerPaintingMode() const;

	void SetActivePaint(UPaperTileSet* TileSet, FIntPoint TopLeft, FIntPoint Dimensions);

	void RefreshBrushSize();
protected:
	bool UseActiveToolAtLocation(const FViewportCursorLocation& Ray);

	bool PaintTiles(const FViewportCursorLocation& Ray);
	bool EraseTiles(const FViewportCursorLocation& Ray);
	bool FloodFillTiles(const FViewportCursorLocation& Ray);


	void UpdatePreviewCursor(const FViewportCursorLocation& Ray);

	// Returns the selected layer under the cursor, and the intersection tile coordinates
	// Note: The tile coordinates can be negative if the brush is off the top or left of the tile map, but still overlaps the map!!!
	UPaperTileLayer* GetSelectedLayerUnderCursor(const FViewportCursorLocation& Ray, int32& OutTileX, int32& OutTileY) const;

	// Compute a world space ray from the screen space mouse coordinates
	FViewportCursorLocation CalculateViewRay(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport);

	static AActor* GetFirstSelectedActorContainingTileMapComponent();

	void CreateModeButtonInModeTray(FToolBarBuilder& Builder);
	
	TSharedRef<FExtender> AddCreationModeExtender(const TSharedRef<FUICommandList> InCommandList);

	void EnableTileMapEditMode();
	bool IsTileMapEditModeActive() const;
protected:
	bool bIsPainting;
	TWeakObjectPtr<UPaperTileSet> PaintSourceTileSet;

	FIntPoint PaintSourceTopLeft;
	FIntPoint PaintSourceDimensions;

	FTransform DrawPreviewSpace;
	FVector DrawPreviewLocation;
	FVector DrawPreviewDimensionsLS;

	int32 EraseBrushSize;

	int32 CursorWidth;
	int32 CursorHeight;
	int32 BrushWidth;
	int32 BrushHeight;

	ETileMapEditorTool::Type ActiveTool;
	ETileMapLayerPaintingMode::Type LayerPaintingMode;
};

