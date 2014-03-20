// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "EdModeTileMap.h"
#include "TileMapEdModeToolkit.h"
#include "LevelEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "../PaperEditorCommands.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// FHorizontalSpan - used for flood filling

struct FHorizontalSpan
{
	int32 X0;
	int32 X1;
	int32 Y;

	FHorizontalSpan(int32 InX, int32 InY)
		: X0(InX)
		, X1(InX)
		, Y(InY)
	{
	}

	// Indexes a bit in the reachability array
	static FBitReference Reach(UPaperTileLayer* Layer, TBitArray<>& Reachability, int32 X, int32 Y)
	{
		const int32 Index = (Layer->LayerWidth * Y) + X;
		return Reachability[Index];
	}

	// Grows a span horizontally until it reaches something that doesn't match
	void GrowSpan(int32 RequiredInk, UPaperTileLayer* Layer, TBitArray<>& Reachability)
	{
		// Go left
		for (int32 TestX = X0 - 1; TestX >= 0; --TestX)
		{
			if ((Layer->GetCell(TestX, Y) == RequiredInk) && !Reach(Layer, Reachability, TestX, Y))
			{
				X0 = TestX;
			}
			else
			{
				break;
			}
		}

		// Go right
		for (int32 TestX = X1 + 1; TestX < Layer->LayerWidth; ++TestX)
		{
			if ((Layer->GetCell(TestX, Y) == RequiredInk) && !Reach(Layer, Reachability, TestX, Y))
			{
				X1 = TestX;
			}
			else
			{
				break;
			}
		}

		// Commit the span to the reachability array
		for (int32 X = X0; X <= X1; ++X)
		{
			Reach(Layer, Reachability, X, Y) = true;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// FEdModeTileMap

const FEditorModeID FEdModeTileMap::EM_TileMap(TEXT("EM_TileMap"));

FEdModeTileMap::FEdModeTileMap()
	: bIsPainting(false)
	, PaintSourceTileSet(NULL)
	, PaintSourceTopLeft(0, 0)
	, PaintSourceDimensions(0, 0)
	, DrawPreviewDimensionsLS(0.0f, 0.0f, 0.0f)
	, EraseBrushSize(1)
{
	ID = EM_TileMap;
	Name = LOCTEXT("TileMapEditMode", "Tile Map Editor");
	bVisible = true;

	SetActiveTool(ETileMapEditorTool::Paintbrush);
	SetActiveLayerPaintingMode(ETileMapLayerPaintingMode::VisualLayers);
}

FEdModeTileMap::~FEdModeTileMap()
{
}

void FEdModeTileMap::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdMode::AddReferencedObjects(Collector);
}

bool FEdModeTileMap::UsesToolkits() const
{
	return true;
}

void FEdModeTileMap::Enter()
{
	if (!Toolkit.IsValid())
	{
		// @todo: Remove this assumption when we make modes per level editor instead of global
		auto ToolkitHost = FModuleManager::LoadModuleChecked< FLevelEditorModule >( "LevelEditor" ).GetFirstLevelEditor();
		Toolkit = MakeShareable(new FTileMapEdModeToolkit);
		Toolkit->Init(ToolkitHost);
	}

	FEdMode::Enter();
}

void FEdModeTileMap::Exit()
{
	FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
	Toolkit.Reset();

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

bool FEdModeTileMap::MouseMove(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, int32 x, int32 y)
{
	if (InViewportClient->EngineShowFlags.ModeWidgets)
	{
		const FViewportCursorLocation Ray = CalculateViewRay(InViewportClient, InViewport);
		UpdatePreviewCursor(Ray);
	}

	// Overridden to prevent the default behavior
	return false;
}

bool FEdModeTileMap::CapturedMouseMove(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY)
{
	if (InViewportClient->EngineShowFlags.ModeWidgets)
	{
		const FViewportCursorLocation Ray = CalculateViewRay(InViewportClient, InViewport);
		
		UpdatePreviewCursor(Ray);

		if (bIsPainting)
		{
			UseActiveToolAtLocation(Ray);
			return true;
		}
		else
		{
			return false;
		}
	}

	return false;
}

bool FEdModeTileMap::StartTracking(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	return true;
}

bool FEdModeTileMap::EndTracking(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	return true;
}

bool FEdModeTileMap::InputKey(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	bool bHandled = false;

	const bool bIsLeftButtonDown = ( InKey == EKeys::LeftMouseButton && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftMouseButton );
	const bool bIsCtrlDown = ( ( InKey == EKeys::LeftControl || InKey == EKeys::RightControl ) && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftControl ) || InViewport->KeyState( EKeys::RightControl );
	const bool bIsShiftDown = ( ( InKey == EKeys::LeftShift || InKey == EKeys::RightShift ) && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftShift ) || InViewport->KeyState( EKeys::RightShift );

	if (InViewportClient->EngineShowFlags.ModeWidgets)
	{
		// Does the user want to paint right now?
		const bool bUserWantsPaint = bIsLeftButtonDown;
		bool bAnyPaintAbleActorsUnderCursor = false;
		bIsPainting = bUserWantsPaint;

		const FViewportCursorLocation Ray = CalculateViewRay(InViewportClient, InViewport);

		UpdatePreviewCursor(Ray);

		if (bIsPainting)
		{
			bHandled = true;
			bAnyPaintAbleActorsUnderCursor = UseActiveToolAtLocation(Ray);
		}

		// Also absorb other mouse buttons, and Ctrl/Alt/Shift events that occur while we're painting as these would cause
		// the editor viewport to start panning/dollying the camera
		{
			const bool bIsOtherMouseButtonEvent = ( InKey == EKeys::MiddleMouseButton || InKey == EKeys::RightMouseButton );
			const bool bCtrlButtonEvent = (InKey == EKeys::LeftControl || InKey == EKeys::RightControl);
			const bool bShiftButtonEvent = (InKey == EKeys::LeftShift || InKey == EKeys::RightShift);
			const bool bAltButtonEvent = (InKey == EKeys::LeftAlt || InKey == EKeys::RightAlt);
			if( bIsPainting && ( bIsOtherMouseButtonEvent || bShiftButtonEvent || bAltButtonEvent ) )
			{
				bHandled = true;
			}

			if (bCtrlButtonEvent && !bIsPainting)
			{
				bHandled = false;
			}
			else if (bIsCtrlDown)
			{
				//default to assuming this is a paint command
				bHandled = true;

				// If no other button was pressed && if a first press and we click OFF of an actor and we will let this pass through so multi-select can attempt to handle it 
				if ((!(bShiftButtonEvent || bAltButtonEvent || bIsOtherMouseButtonEvent)) && ((InKey == EKeys::LeftMouseButton) && ((InEvent == IE_Pressed) || (InEvent == IE_Released)) && (!bAnyPaintAbleActorsUnderCursor)))
				{
					bHandled = false;
					bIsPainting = false;
				}

				// Allow Ctrl+B to pass through so we can support the finding of a selected static mesh in the content browser.
				if ( !(bShiftButtonEvent || bAltButtonEvent || bIsOtherMouseButtonEvent) && ( (InKey == EKeys::B) && (InEvent == IE_Pressed) ) )
				{
					bHandled = false;
				}

				// If we are not painting, we will let the CTRL-Z and CTRL-Y key presses through to support undo/redo.
				if (!bIsPainting && ((InKey == EKeys::Z) || (InKey == EKeys::Y)))
				{
					bHandled = false;
				}
			}
		}
	}

	return bHandled;
}

bool FEdModeTileMap::InputDelta(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	return false;
}

void FEdModeTileMap::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

	//@TODO: Need the force-realtime hack

	// If this viewport does not support Mode widgets we will not draw it here.
	FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)Viewport->GetClient();
	if ((ViewportClient != NULL) && !ViewportClient->EngineShowFlags.ModeWidgets)
	{
		return;
	}

	// Make sure the cursor is visible OR we're flood filling.  No point drawing a paint cue when there's no cursor.
// 	if( Viewport->IsCursorVisible() || IsForceRendered())
// 	{
// 		if( !PDI->IsHitTesting() )
// 		{
// 		}
// 	}

	// Draw the preview cursor
	if (!DrawPreviewDimensionsLS.IsNearlyZero())
	{
		FVector X = DrawPreviewSpace.GetScaledAxis(EAxis::X);
		FVector Y = DrawPreviewSpace.GetScaledAxis(EAxis::Y);
		FVector Z = DrawPreviewSpace.GetScaledAxis(EAxis::Z);
		FVector Base = DrawPreviewLocation;

		DrawOrientedWireBox(PDI, Base, X, Y, Z, DrawPreviewDimensionsLS, FLinearColor::White, SDPG_Foreground);
	}
}

void FEdModeTileMap::DrawHUD(FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FString InkInfo = FString::Printf(TEXT("Ink: (%d, %d)  %dx%d  %s"), PaintSourceTopLeft.X, PaintSourceTopLeft.Y, PaintSourceDimensions.X, PaintSourceDimensions.Y, 
		(PaintSourceTileSet.Get() != NULL) ? (*PaintSourceTileSet.Get()->GetName()) : TEXT("(null)"));

	FCanvasTextItem Msg(FVector2D(10, 30), FText::FromString(InkInfo), GEngine->GetMediumFont(), FLinearColor::White);
	Canvas->DrawItem(Msg);
}

bool FEdModeTileMap::AllowWidgetMove()
{
	return false;
}

bool FEdModeTileMap::ShouldDrawWidget() const
{
	return false;
}

bool FEdModeTileMap::UsesTransformWidget() const
{
	return false;
}

UPaperTileLayer* FEdModeTileMap::GetSelectedLayerUnderCursor(const FViewportCursorLocation& Ray, int32& OutTileX, int32& OutTileY) const
{
	const FVector TraceStart = Ray.GetOrigin();
	const FVector TraceDir = Ray.GetDirection();

	const bool bCollisionPainting = (GetActiveLayerPaintingMode() == ETileMapLayerPaintingMode::CollisionLayers);

	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = CastChecked<AActor>(*Iter);
		if (UPaperTileMapRenderComponent* TileMap = Actor->FindComponentByClass<UPaperTileMapRenderComponent>())
		{
			// Find the first visible layer
			int32 LayerIndex = 0;
			for (; LayerIndex < TileMap->TileLayers.Num(); ++LayerIndex)
			{
				UPaperTileLayer* Layer = TileMap->TileLayers[LayerIndex];
				if (!Layer->bHiddenInEditor && Layer->bCollisionLayer == bCollisionPainting)
				{
					break;
				}
			}

			// If there was a visible layer, pick it
			if (LayerIndex < TileMap->TileLayers.Num())
			{
				UPaperTileLayer* Layer = TileMap->TileLayers[LayerIndex];

				const float WX = TileMap->MapWidth * TileMap->TileWidth;
				const float WY = TileMap->MapHeight * TileMap->TileHeight;

				const FTransform ComponentToWorld = TileMap->ComponentToWorld;
				FVector LocalStart = ComponentToWorld.InverseTransformPosition(TraceStart);
				FVector LocalDirection = ComponentToWorld.InverseTransformVector(TraceDir);

				FVector Intersection;
				FPlane Plane(FVector(1, 0, 0), FVector::ZeroVector, FVector(0, 0, 1));

				if (FMath::SegmentPlaneIntersection(LocalStart, LocalDirection * HALF_WORLD_MAX, Plane, /*out*/ Intersection))
				{
					//@TODO: Ideally tile pivots weren't in the center!
					const float NormalizedX = (Intersection.X + 0.5f * TileMap->TileWidth) / WX;
					const float NormalizedY = (-Intersection.Z + 0.5f * TileMap->TileHeight) / WY;

					OutTileX = FMath::Floor(NormalizedX * TileMap->MapWidth);
					OutTileY = FMath::Floor(NormalizedY * TileMap->MapHeight);
					
					if ((OutTileX > -BrushWidth) && (OutTileX < TileMap->MapWidth) && (OutTileY > -BrushHeight) && (OutTileY < TileMap->MapHeight))
						//(NormalizedX >= 0.0f) && (NormalizedX < 1.0f) && (NormalizedY >= 0.0f) && (NormalizedY < 1.0f))
					{
						//OutTileX = FMath::Clamp<int>(NormalizedX * TileMap->MapWidth, 0, TileMap->MapWidth-1);
						//OutTileY = FMath::Clamp<int>(NormalizedY * TileMap->MapHeight, 0, TileMap->MapHeight-1);
						return Layer;
					}
				}
			}
		}
	}

	OutTileX = 0;
	OutTileY = 0;
	return NULL;
}


bool FEdModeTileMap::UseActiveToolAtLocation(const FViewportCursorLocation& Ray)
{
	switch (ActiveTool)
	{
	case ETileMapEditorTool::Paintbrush:
		return PaintTiles(Ray);
	case ETileMapEditorTool::Eraser:
		return EraseTiles(Ray);
		break;
	case ETileMapEditorTool::PaintBucket:
		return FloodFillTiles(Ray);
	default:
		check(false);
		return false;
	};
}

bool FEdModeTileMap::PaintTiles(const FViewportCursorLocation& Ray)
{
	bool bPaintedOnSomething = false;
	bool bChangedSomething = false;

	// Validate that the tool we're using can be used right now
	if ((BrushWidth <= 0) || (BrushHeight <= 0))
	{
		return false;
	}

	// If we are using an ink source, validate that it exists
	UPaperTileSet* InkSource = NULL;
	if ( GetActiveLayerPaintingMode() != ETileMapLayerPaintingMode::CollisionLayers )
	{
		InkSource = PaintSourceTileSet.Get();
		if ( !InkSource )
		{
			return false;
		}
	}

	int32 DestTileX;
	int32 DestTileY;

	if (UPaperTileLayer* Layer = GetSelectedLayerUnderCursor(Ray, /*out*/ DestTileX, /*out*/ DestTileY))
	{
		UPaperTileMapRenderComponent* TileMap = CastChecked<UPaperTileMapRenderComponent>(Layer->GetOuter());

		FScopedTransaction Transaction( LOCTEXT("TileMapPaintAction", "Tile Painting") );

		for (int32 Y = 0; Y < BrushHeight; ++Y)
		{
			const int32 DY = DestTileY + Y;

			if ((DY < 0) || (DY >= TileMap->MapHeight))
			{
				continue;
			}

			for (int32 X = 0; X < BrushWidth; ++X)
			{
				const int32 DX = DestTileX + X;

				if ((DX < 0) || (DX >= TileMap->MapWidth))
				{
					continue;
				}

				int32 Ink = 0;
				if (GetActiveLayerPaintingMode() == ETileMapLayerPaintingMode::CollisionLayers)
				{
					// 1 Means collision, 0 means no collision
					Ink = 1;
				}
				else
				{
					const int32 SY = PaintSourceTopLeft.Y + Y;
					const int32 SX = PaintSourceTopLeft.X + X;

					if ( (SY >= InkSource->GetTileCountY()) )
					{
						continue;
					}

					if ( (SX >= InkSource->GetTileCountX()) )
					{
						continue;
					}

					Ink = SX + (SY * InkSource->GetTileCountX());
				}

				if (Layer->GetCell(DX, DY) != Ink)
				{
					if (!bChangedSomething)
					{
						Layer->SetFlags(RF_Transactional);
						Layer->Modify();
						bChangedSomething = true;
					}
					Layer->SetCell(DX, DY, Ink);
				}

				bPaintedOnSomething = true;
			}
		}

		if ( bChangedSomething && GetActiveLayerPaintingMode() == ETileMapLayerPaintingMode::CollisionLayers )
		{
			TileMap->PostEditChange();
		}

		if (!bChangedSomething)
		{
			Transaction.Cancel();
		}
	}

	return bPaintedOnSomething;
}

bool FEdModeTileMap::EraseTiles(const FViewportCursorLocation& Ray)
{
	bool bPaintedOnSomething = false;
	bool bChangedSomething = false;

	const int32 EmptyCellValue = 0; //@TODO: Would really like to use INDEX_NONE for this...

	int32 DestTileX;
	int32 DestTileY;

	if (UPaperTileLayer* Layer = GetSelectedLayerUnderCursor(Ray, /*out*/ DestTileX, /*out*/ DestTileY))
	{
		UPaperTileMapRenderComponent* TileMap = CastChecked<UPaperTileMapRenderComponent>(Layer->GetOuter());

		FScopedTransaction Transaction( LOCTEXT("TileMapEraseAction", "Tile Erasing") );

		for (int32 Y = 0; Y < BrushWidth; ++Y)
		{
			const int DY = DestTileY + Y;

			if ((DY < 0) || (DY >= TileMap->MapHeight))
			{
				continue;
			}

			for (int32 X = 0; X < BrushHeight; ++X)
			{
				const int DX = DestTileX + X;

				if ((DX < 0) || (DX >= TileMap->MapWidth))
				{
					continue;
				}

				if (Layer->GetCell(DX, DY) != EmptyCellValue)
				{
					if (!bChangedSomething)
					{
						Layer->SetFlags(RF_Transactional);
						Layer->Modify();
						bChangedSomething = true;
					}
					Layer->SetCell(DX, DY, EmptyCellValue);
				}

				bPaintedOnSomething = true;
			}
		}

		if ( bChangedSomething && GetActiveLayerPaintingMode() == ETileMapLayerPaintingMode::CollisionLayers )
		{
			TileMap->PostEditChange();
		}

		if (!bChangedSomething)
		{
			Transaction.Cancel();
		}
	}

	return bPaintedOnSomething;
}

bool FEdModeTileMap::FloodFillTiles(const FViewportCursorLocation& Ray)
{
	bool bPaintedOnSomething = false;
	bool bChangedSomething = false;

	// Validate that the tool we're using can be used right now
	if ((BrushWidth <= 0) || (BrushHeight <= 0))
	{
		return false;
	}

	// If we are using an ink source, validate that it exists
	UPaperTileSet* InkSource = NULL;
	if ( GetActiveLayerPaintingMode() != ETileMapLayerPaintingMode::CollisionLayers )
	{
		InkSource = PaintSourceTileSet.Get();
		if ( !InkSource )
		{
			return false;
		}
	}

	int DestTileX;
	int DestTileY;

	if (UPaperTileLayer* Layer = GetSelectedLayerUnderCursor(Ray, /*out*/ DestTileX, /*out*/ DestTileY))
	{
		//@TODO: Should we allow off-canvas flood filling too?
		if ((DestTileX < 0) || (DestTileY < 0))
		{
			return false;
		}

		// The kind of ink we'll replace, starting at the seed point
		const int32 RequiredInk = Layer->GetCell(DestTileX, DestTileY);

		UPaperTileMapRenderComponent* TileMap = CastChecked<UPaperTileMapRenderComponent>(Layer->GetOuter());

		//@TODO: Unoptimized first-pass approach
		const int32 NumTiles = TileMap->MapWidth * TileMap->MapHeight;

		// Flag for all tiles indicating if they are reachable from the seed paint point
		TBitArray<> TileReachability;
		TileReachability.Init(false, NumTiles);

		// List of horizontal spans that still need to be checked for adjacent colors above and below
		TArray<FHorizontalSpan> OutstandingSpans;

		// Start off at the seed point
		FHorizontalSpan& InitialSpan = *(new (OutstandingSpans) FHorizontalSpan(DestTileX, DestTileY));
		InitialSpan.GrowSpan(RequiredInk, Layer, TileReachability);

		// Process the list of outstanding spans until it is empty
		while (OutstandingSpans.Num())
		{
			FHorizontalSpan Span = OutstandingSpans.Pop();

			// Create spans below and above
			for (int32 DY = -1; DY <= 1; DY += 2)
			{
				const int32 Y = Span.Y + DY;
				if ((Y < 0) || (Y >= Layer->LayerHeight))
				{
					continue;
				}
					
				for (int32 X = Span.X0; X <= Span.X1; ++X)
				{
					// If it is the right color and not already visited, create a span there
					if ((Layer->GetCell(X, Y) == RequiredInk) && !FHorizontalSpan::Reach(Layer, TileReachability, X, Y))
					{
						FHorizontalSpan& NewSpan = *(new (OutstandingSpans) FHorizontalSpan(X, Y));
						NewSpan.GrowSpan(RequiredInk, Layer, TileReachability);
					}
				}
			}
		}
		
		// Now the reachability map should be populated, so we can use it to flood fill
		FScopedTransaction Transaction( LOCTEXT("TileMapFloodFillAction", "Tile Paint Bucket") );

		// Figure out where the top left square of the map starts in the pattern, based on the seed point
		int32 BrushPatternOffsetX = (BrushWidth - ((DestTileX + BrushWidth) % BrushWidth));
		int32 BrushPatternOffsetY = (BrushHeight - ((DestTileY + BrushHeight) % BrushHeight));
			
		int32 ReachIndex = 0;
		for (int32 DY = 0; DY < TileMap->MapHeight; ++DY)
		{
			const int32 InsideBrushY = (DY + BrushPatternOffsetY) % BrushHeight;
	
			for (int32 DX = 0; DX < TileMap->MapWidth; ++DX)
			{
				if (TileReachability[ReachIndex++])
				{
					const int32 InsideBrushX = (DX + BrushPatternOffsetX) % BrushWidth;

					int32 NewInk = 0;
					if ( GetActiveLayerPaintingMode() == ETileMapLayerPaintingMode::CollisionLayers )
					{
						NewInk = 1;
					}
					else
					{
						const int32 TileSetX = PaintSourceTopLeft.X + InsideBrushX;
						const int32 TileSetY = PaintSourceTopLeft.Y + InsideBrushY;
						NewInk = TileSetX + (TileSetY * InkSource->GetTileCountX());
					}

					if (Layer->GetCell(DX, DY) != NewInk)
					{
						if (!bChangedSomething)
						{
							Layer->SetFlags(RF_Transactional);
							Layer->Modify();
							bChangedSomething = true;
						}
						Layer->SetCell(DX, DY, NewInk);
					}

					bPaintedOnSomething = true;
				}
			}
		}

		if ( bChangedSomething && GetActiveLayerPaintingMode() == ETileMapLayerPaintingMode::CollisionLayers )
		{
			TileMap->PostEditChange();
		}

		if (!bChangedSomething)
		{
			Transaction.Cancel();
		}
	}

	return bPaintedOnSomething;
}

void FEdModeTileMap::SetActivePaint(UPaperTileSet* TileSet, FIntPoint TopLeft, FIntPoint Dimensions)
{
	PaintSourceTileSet = TileSet;
	PaintSourceTopLeft = TopLeft;
	PaintSourceDimensions = Dimensions;
	RefreshBrushSize();
}

void FEdModeTileMap::UpdatePreviewCursor(const FViewportCursorLocation& Ray)
{
	DrawPreviewDimensionsLS = FVector::ZeroVector;

	// See if we should draw the preview
	{
		int32 LocalTileX0;
		int32 LocalTileY0;
		if (UPaperTileLayer* TileLayer = GetSelectedLayerUnderCursor(Ray, /*out*/ LocalTileX0, /*out*/ LocalTileY0))
		{
			const int32 LocalTileX1 = LocalTileX0 + CursorWidth;
			const int32 LocalTileY1 = LocalTileY0 + CursorHeight;

			UPaperTileMapRenderComponent* TileMap = CastChecked<UPaperTileMapRenderComponent>(TileLayer->GetOuter());
			const FVector WorldPosition = TileMap->ConvertTilePositionToWorldSpace(LocalTileX0, LocalTileY0);
			const FVector WorldPositionBR = TileMap->ConvertTilePositionToWorldSpace(LocalTileX1, LocalTileY1);

			DrawPreviewSpace = TileMap->ComponentToWorld;
			DrawPreviewLocation = (WorldPosition + WorldPositionBR) * 0.5f;

			DrawPreviewDimensionsLS = 0.5f*FVector(CursorWidth * TileMap->TileWidth, 0.0f, -CursorHeight * TileMap->TileHeight);
		}
	}
}

FViewportCursorLocation FEdModeTileMap::CalculateViewRay(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( 
		InViewportClient->Viewport, 
		InViewportClient->GetScene(),
		InViewportClient->EngineShowFlags )
		.SetRealtimeUpdate( InViewportClient->IsRealtime() ));

	FSceneView* View = InViewportClient->CalcSceneView( &ViewFamily );
	FViewportCursorLocation MouseViewportRay( View, (FLevelEditorViewportClient*)InViewport->GetClient(), InViewport->GetMouseX(), InViewport->GetMouseY() );

	return MouseViewportRay;
}

void FEdModeTileMap::PeekAtSelectionChangedEvent(UObject* ItemUndergoingChange)
{
	/*
	// Auto-switch to/from tile map editing mode
	const bool bModeCurrentlyActive = GEditorModeTools().IsModeActive(EM_TileMap);
	const AActor* TileMapActor = GetFirstSelectedActorContainingTileMapComponent();

	if ((TileMapActor == NULL) && bModeCurrentlyActive)
	{
		GEditorModeTools().DeactivateMode(EM_TileMap);
	}
	else if ((TileMapActor != NULL) && !bModeCurrentlyActive)
	{
		GEditorModeTools().ActivateMode(EM_TileMap);
	}
	*/
}

AActor* FEdModeTileMap::GetFirstSelectedActorContainingTileMapComponent()
{
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			if (Actor->FindComponentByClass<UPaperTileMapRenderComponent>() != NULL)
			{
				return Actor;
			}
		}
	}

	return NULL;
}

void FEdModeTileMap::CreateModeButtonInModeTray(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton( FPaperEditorCommands::Get().EnterTileMapEditMode );
}

void FEdModeTileMap::SetActiveTool(ETileMapEditorTool::Type NewTool)
{
	ActiveTool = NewTool;
	RefreshBrushSize();
}

ETileMapEditorTool::Type FEdModeTileMap::GetActiveTool() const
{
	return ActiveTool;
}

void FEdModeTileMap::SetActiveLayerPaintingMode(ETileMapLayerPaintingMode::Type NewMode)
{
	LayerPaintingMode = NewMode;
	RefreshBrushSize();
}

ETileMapLayerPaintingMode::Type FEdModeTileMap::GetActiveLayerPaintingMode() const
{
	return LayerPaintingMode;
}

void FEdModeTileMap::RefreshBrushSize()
{
	BrushWidth = 1;
	BrushHeight = 1;
	CursorWidth = 1;
	CursorHeight = 1;

	if ( GetActiveLayerPaintingMode() != ETileMapLayerPaintingMode::CollisionLayers )
	{
		switch (ActiveTool)
		{
		case ETileMapEditorTool::Paintbrush:
			BrushWidth = PaintSourceDimensions.X;
			BrushHeight = PaintSourceDimensions.Y;
			CursorWidth = FMath::Max(1, PaintSourceDimensions.X);
			CursorHeight = FMath::Max(1, PaintSourceDimensions.Y);
			break;
		case ETileMapEditorTool::Eraser:
			BrushWidth = EraseBrushSize;
			BrushHeight = EraseBrushSize;
			CursorWidth = EraseBrushSize;
			CursorHeight = EraseBrushSize;
			break;
		case ETileMapEditorTool::PaintBucket:
			BrushWidth = PaintSourceDimensions.X;
			BrushHeight = PaintSourceDimensions.Y;
			CursorWidth = 1;
			CursorHeight = 1;
			break;
		default:
			check(false);
			break;
		}
	}


}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE