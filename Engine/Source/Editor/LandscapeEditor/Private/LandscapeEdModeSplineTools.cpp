// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "ObjectTools.h"
#include "LandscapeEdMode.h"
#include "ScopedTransaction.h"
#include "EngineTerrainClasses.h"
#include "EngineFoliageClasses.h"
#include "Landscape/LandscapeEdit.h"
#include "Landscape/LandscapeRender.h"
#include "Landscape/LandscapeDataAccess.h"
#include "Landscape/LandscapeSplineProxies.h"
#include "LandscapeEditorModule.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "LandscapeEdModeTools.h"
#include "Components/SplineMeshComponent.h"
#include "LandscapeSplineImportExport.h"


#define LOCTEXT_NAMESPACE "Landscape"

//
// FLandscapeToolSplines
//
class FLandscapeToolSplines : public FLandscapeTool, public FEditorUndoClient
{
public:
	FLandscapeToolSplines(class FEdModeLandscape* InEdMode)
		: EdMode(InEdMode)
		, LandscapeInfo(NULL)
		, SelectedSplineControlPoints()
		, SelectedSplineSegments()
		, DraggingTangent_Segment(NULL)
		, DraggingTangent_End(false)
		, bMovingControlPoint(false)
		, bAutoRotateOnJoin(true)
		, bAutoChangeConnectionsOnMove(true)
		, bDeleteLooseEnds(false)
		, bCopyMeshToNewControlPoint(false)
	{
		// Register to update when an undo/redo operation has been called to update our list of actors
		GEditor->RegisterForUndo(this);
	}

	~FLandscapeToolSplines()
	{
		// GEditor is invalid at shutdown as the object system is unloaded before the landscape module.
		if( UObjectInitialized() )
		{
			// Remove undo delegate
			GEditor->UnregisterForUndo(this);
		}
	}

	virtual const TCHAR* GetToolName() OVERRIDE { return TEXT("Splines"); }
	virtual FText GetDisplayName() OVERRIDE { return NSLOCTEXT("UnrealEd", "LandscapeMode_Splines", "Splines"); };

	virtual void SetEditRenderType() OVERRIDE { GLandscapeEditRenderMode = ELandscapeEditRenderMode::None | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask); }
	virtual bool SupportsMask() OVERRIDE { return false; }

	virtual bool IsValidForTarget(const FLandscapeToolTarget& Target) OVERRIDE
	{
		return true; // applied to all...
	}

	void CreateSplineComponent( ALandscape* Landscape ) 
	{
		Landscape->SplineComponent = ConstructObject<ULandscapeSplinesComponent>(ULandscapeSplinesComponent::StaticClass(), Landscape, NAME_None, RF_Transactional);
		Landscape->SplineComponent->RelativeScale3D = FVector(1.0f) / Landscape->GetRootComponent()->RelativeScale3D;
		Landscape->SplineComponent->AttachTo(Landscape->GetRootComponent());
		Landscape->SplineComponent->ShowSplineEditorMesh(true);
	}

	void UpdatePropertiesWindows()
	{
		if( GEditorModeTools().IsModeActive( EdMode->GetID() ) )
		{
			TArray<UObject*> Objects;
			Objects.Reset(SelectedSplineControlPoints.Num() + SelectedSplineSegments.Num());

			for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
			{
				Objects.Add(*It);
			}
			for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
			{
				Objects.Add(*It);
			}


			FPropertyEditorModule& PropertyModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>( TEXT("PropertyEditor") );
			PropertyModule.UpdatePropertyViews(Objects);
		}
	}

	void ClearSelectedControlPoints()
	{
		for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
		{
			checkSlow((*It)->IsSplineSelected());
			(*It)->Modify();
			(*It)->SetSplineSelected(false);
		}
		SelectedSplineControlPoints.Empty();
	}

	void ClearSelectedSegments()
	{
		for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
		{
			checkSlow((*It)->IsSplineSelected());
			(*It)->Modify();
			(*It)->SetSplineSelected(false);
		}
		SelectedSplineSegments.Empty();
	}

	void ClearSelection()
	{
		ClearSelectedControlPoints();
		ClearSelectedSegments();
	}

	void DeselectControlPoint(ULandscapeSplineControlPoint* ControlPoint)
	{
		checkSlow(ControlPoint->IsSplineSelected());
		SelectedSplineControlPoints.Remove(ControlPoint);
		ControlPoint->Modify();
		ControlPoint->SetSplineSelected(false);
	}

	void DeSelectSegment(ULandscapeSplineSegment* Segment)
	{
		checkSlow(Segment->IsSplineSelected());
		SelectedSplineSegments.Remove(Segment);
		Segment->Modify();
		Segment->SetSplineSelected(false);
	}

	void SelectControlPoint(ULandscapeSplineControlPoint* ControlPoint)
	{
		checkSlow(!ControlPoint->IsSplineSelected());
		SelectedSplineControlPoints.Add(ControlPoint);
		ControlPoint->Modify();
		ControlPoint->SetSplineSelected(true);
	}

	void SelectSegment(ULandscapeSplineSegment* Segment)
	{
		checkSlow(!Segment->IsSplineSelected());
		SelectedSplineSegments.Add(Segment);
		Segment->Modify();
		Segment->SetSplineSelected(true);

		GEditorModeTools().SetWidgetMode(FWidget::WM_Scale);
	}

	void SelectConnected()
	{
		TArray<ULandscapeSplineControlPoint*> ControlPointsToProcess = SelectedSplineControlPoints.Array();

		while (ControlPointsToProcess.Num() > 0)
		{
			const ULandscapeSplineControlPoint* ControlPoint = ControlPointsToProcess.Pop();

			for (int32 i = 0; i < ControlPoint->ConnectedSegments.Num(); i++)
			{
				const FLandscapeSplineConnection& Connection = ControlPoint->ConnectedSegments[i];
				ULandscapeSplineControlPoint* OtherEnd = Connection.GetFarConnection().ControlPoint;

				if (!OtherEnd->IsSplineSelected())
				{
					SelectControlPoint(OtherEnd);
					ControlPointsToProcess.Add(OtherEnd);
				}
			}
		}

		TArray<ULandscapeSplineSegment*> SegmentsToProcess = SelectedSplineSegments.Array();

		while (SegmentsToProcess.Num() > 0)
		{
			const ULandscapeSplineSegment* Segment = SegmentsToProcess.Pop();

			for (int32 End = 0; End <= 1; End++)
			{
				const ULandscapeSplineControlPoint* ControlPoint = Segment->Connections[End].ControlPoint;

				for (int32 i = 0; i < ControlPoint->ConnectedSegments.Num(); i++)
				{
					const FLandscapeSplineConnection& Connection = ControlPoint->ConnectedSegments[i];

					if (Connection.Segment != Segment && !Connection.Segment->IsSplineSelected())
					{
						SelectSegment(Connection.Segment);
						SegmentsToProcess.Add(Connection.Segment);
					}
				}
			}
		}
	}

	void SelectAdjacentControlPoints()
	{
		for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
		{
			const ULandscapeSplineSegment* Segment = *It;
			if (!Segment->Connections[0].ControlPoint->IsSplineSelected())
			{
				SelectControlPoint(Segment->Connections[0].ControlPoint);
			}
			if (!Segment->Connections[1].ControlPoint->IsSplineSelected())
			{
				SelectControlPoint(Segment->Connections[1].ControlPoint);
			}
		}
	}

	void SelectAdjacentSegments()
	{
		for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
		{
			const ULandscapeSplineControlPoint* ControlPoint = *It;
			for (int32 i = 0; i < ControlPoint->ConnectedSegments.Num(); i++)
			{
				const FLandscapeSplineConnection& Connection = ControlPoint->ConnectedSegments[i];

				if (!Connection.Segment->IsSplineSelected())
				{
					SelectSegment(Connection.Segment);
				}
			}
		}
	}

	void AddSegment(ULandscapeSplineControlPoint* Start, ULandscapeSplineControlPoint* End, bool bAutoRotateStart, bool bAutoRotateEnd)
	{
		FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_AddSegment", "Add Landscape Spline Segment") );

		if (Start == End)
		{
			//UE_LOG( TEXT("Can't join spline control point to itself.") );
			return;
		}

		if (Start->GetOuterULandscapeSplinesComponent() != End->GetOuterULandscapeSplinesComponent())
		{
			//UE_LOG( TEXT("Can't join spline control points across different terrains.") );
			return;
		}

		for (int32 i = 0; i < Start->ConnectedSegments.Num(); i++)
		{
			const FLandscapeSplineConnection& Connection = Start->ConnectedSegments[i];

			// if the *other* end on the connected segment connects to the "end" control point...
			if (Connection.GetFarConnection().ControlPoint == End)
			{
				//UE_LOG( TEXT("Spline control points already joined connected!") );
				return;
			}
		}

		ULandscapeSplinesComponent* SplinesComponent = Start->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		Start->Modify();
		End->Modify();

		ULandscapeSplineSegment* NewSegment = ConstructObject<ULandscapeSplineSegment>(ULandscapeSplineSegment::StaticClass(), SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->Segments.Add(NewSegment);

		NewSegment->Connections[0].ControlPoint = Start;
		NewSegment->Connections[1].ControlPoint = End;

		NewSegment->Connections[0].SocketName = Start->GetBestConnectionTo(End->Location);
		NewSegment->Connections[1].SocketName = End->GetBestConnectionTo(Start->Location);

		FVector StartLocation; FRotator StartRotation;
		Start->GetConnectionLocationAndRotation(NewSegment->Connections[0].SocketName, StartLocation, StartRotation);
		FVector EndLocation; FRotator EndRotation;
		End->GetConnectionLocationAndRotation(NewSegment->Connections[1].SocketName, EndLocation, EndRotation);

		// Set up tangent lengths
		NewSegment->Connections[0].TangentLen = (EndLocation - StartLocation).Size();
		NewSegment->Connections[1].TangentLen = NewSegment->Connections[0].TangentLen;

		NewSegment->AutoFlipTangents();

		// set up other segment options
		if (Start->ConnectedSegments.Num() > 0)
		{
			NewSegment->LayerName        = Start->ConnectedSegments[0].Segment->LayerName;
			NewSegment->SplineMeshes     = Start->ConnectedSegments[0].Segment->SplineMeshes;
			NewSegment->bRaiseTerrain    = Start->ConnectedSegments[0].Segment->bRaiseTerrain;
			NewSegment->bLowerTerrain    = Start->ConnectedSegments[0].Segment->bLowerTerrain;
			NewSegment->bEnableCollision = Start->ConnectedSegments[0].Segment->bEnableCollision;
			NewSegment->bCastShadow      = Start->ConnectedSegments[0].Segment->bCastShadow;
		}
		else if (End->ConnectedSegments.Num() > 0)
		{
			NewSegment->LayerName        = End->ConnectedSegments[0].Segment->LayerName;
			NewSegment->SplineMeshes     = End->ConnectedSegments[0].Segment->SplineMeshes;
			NewSegment->bRaiseTerrain    = End->ConnectedSegments[0].Segment->bRaiseTerrain;
			NewSegment->bLowerTerrain    = End->ConnectedSegments[0].Segment->bLowerTerrain;
			NewSegment->bEnableCollision = End->ConnectedSegments[0].Segment->bEnableCollision;
			NewSegment->bCastShadow      = End->ConnectedSegments[0].Segment->bCastShadow;
		}
		else
		{
			// Use defaults
		}

		Start->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 0));
		End->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 1));

		if (bAutoRotateStart)
		{
			Start->AutoCalcRotation();
			Start->UpdateSplinePoints();
		}
		if (bAutoRotateEnd)
		{
			End->AutoCalcRotation();
			End->UpdateSplinePoints();
		}

		// Control points' points are currently based on connected segments, so need to be updated.
		if (Start->Mesh != NULL)
		{
			Start->UpdateSplinePoints();
		}
		if (End->Mesh != NULL)
		{
			Start->UpdateSplinePoints();
		}
		NewSegment->UpdateSplinePoints();
	}

	void AddControlPoint(ALandscape* Landscape, const FVector& LocalLocation)
	{
		FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_AddControlPoint", "Add Landscape Spline Control Point") );

		Landscape->SplineComponent->Modify();

		ULandscapeSplineControlPoint* NewControlPoint = ConstructObject<ULandscapeSplineControlPoint>(ULandscapeSplineControlPoint::StaticClass(), Landscape->SplineComponent, NAME_None, RF_Transactional);
		Landscape->SplineComponent->ControlPoints.Add(NewControlPoint);

		NewControlPoint->Location = LocalLocation;

		if (SelectedSplineControlPoints.Num() > 0)
		{
			ULandscapeSplineControlPoint* FirstPoint = *SelectedSplineControlPoints.CreateConstIterator();
			NewControlPoint->Rotation = (NewControlPoint->Location - FirstPoint->Location).Rotation();
			NewControlPoint->Width = FirstPoint->Width;
			NewControlPoint->SideFalloff = FirstPoint->SideFalloff;
			NewControlPoint->EndFalloff = FirstPoint->EndFalloff;

			if (bCopyMeshToNewControlPoint)
			{
				NewControlPoint->Mesh = FirstPoint->Mesh;
				NewControlPoint->MeshScale = FirstPoint->MeshScale;
				NewControlPoint->bEnableCollision = FirstPoint->bEnableCollision;
				NewControlPoint->bCastShadow = FirstPoint->bCastShadow;
			}

			for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
			{
				AddSegment(*It, NewControlPoint, bAutoRotateOnJoin, true);
			}
		}

		ClearSelection();
		SelectControlPoint(NewControlPoint);
		UpdatePropertiesWindows();

		if (!Landscape->SplineComponent->IsRegistered())
		{
			Landscape->SplineComponent->RegisterComponent();
		}
		else
		{
			Landscape->SplineComponent->MarkRenderStateDirty();
		}
	}

	void DeleteSegment(ULandscapeSplineSegment* ToDelete, bool bDeleteLooseEnds)
	{
		FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_DeleteSegment", "Delete Landscape Spline Segment") );

		ULandscapeSplinesComponent* SplinesComponent = ToDelete->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();

		ToDelete->Modify();
		ToDelete->DeleteSplinePoints();

		ToDelete->Connections[0].ControlPoint->Modify();
		ToDelete->Connections[1].ControlPoint->Modify();
		ToDelete->Connections[0].ControlPoint->ConnectedSegments.Remove(FLandscapeSplineConnection(ToDelete, 0));
		ToDelete->Connections[1].ControlPoint->ConnectedSegments.Remove(FLandscapeSplineConnection(ToDelete, 1));

		if (bDeleteLooseEnds)
		{
			if (ToDelete->Connections[0].ControlPoint->ConnectedSegments.Num() == 0)
			{
				SplinesComponent->ControlPoints.Remove(ToDelete->Connections[0].ControlPoint);
			}
			if (ToDelete->Connections[1].ControlPoint != ToDelete->Connections[0].ControlPoint
				&& ToDelete->Connections[1].ControlPoint->ConnectedSegments.Num() == 0)
			{
				SplinesComponent->ControlPoints.Remove(ToDelete->Connections[1].ControlPoint);
			}
		}

		SplinesComponent->Segments.Remove(ToDelete);

		// Control points' points are currently based on connected segments, so need to be updated.
		if (ToDelete->Connections[0].ControlPoint->Mesh != NULL)
		{
			ToDelete->Connections[0].ControlPoint->UpdateSplinePoints();
		}
		if (ToDelete->Connections[1].ControlPoint->Mesh != NULL)
		{
			ToDelete->Connections[1].ControlPoint->UpdateSplinePoints();
		}

		SplinesComponent->MarkRenderStateDirty();
	}

	void DeleteControlPoint(ULandscapeSplineControlPoint* ToDelete, bool bDeleteLooseEnds)
	{
		FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_DeleteControlPoint", "Delete Landscape Spline Control Point") );

		ULandscapeSplinesComponent* SplinesComponent = ToDelete->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();

		ToDelete->Modify();
		ToDelete->DeleteSplinePoints();

		if (ToDelete->ConnectedSegments.Num() == 2
			&& ToDelete->ConnectedSegments[0].Segment != ToDelete->ConnectedSegments[1].Segment)
		{
			int32 Result = FMessageDialog::Open( EAppMsgType::YesNoCancel, LOCTEXT("WantToJoinControlPoint", "Control point has two segments attached, do you want to join them?"));
			switch (Result)
			{
			case EAppReturnType::Yes:
				{
					// Copy the other end of connection 1 into the near end of connection 0, then delete connection 1
					TArray<FLandscapeSplineConnection>& Connections = ToDelete->ConnectedSegments;
					Connections[0].Segment->Modify();
					Connections[1].Segment->Modify();

					Connections[0].GetNearConnection() = Connections[1].GetFarConnection();
					Connections[0].Segment->UpdateSplinePoints();

					Connections[1].Segment->DeleteSplinePoints();

					// Get the control point at the *other* end of the segment and remove it from it
					ULandscapeSplineControlPoint* OtherEnd = Connections[1].GetFarConnection().ControlPoint;
					OtherEnd->Modify();

					FLandscapeSplineConnection* OtherConnection = OtherEnd->ConnectedSegments.FindByKey(FLandscapeSplineConnection(Connections[1].Segment, 1 - Connections[1].End));
					*OtherConnection = FLandscapeSplineConnection(Connections[0].Segment, Connections[0].End);

					SplinesComponent->Segments.Remove(Connections[1].Segment);

					ToDelete->ConnectedSegments.Empty();

					SplinesComponent->ControlPoints.Remove(ToDelete);
					SplinesComponent->MarkRenderStateDirty();

					return;
				}
				break;
			case EAppReturnType::No:
				// Use the "delete all segments" code below
				break;
			case EAppReturnType::Cancel:
				// Do nothing
				return;
			}
		}

		for (int32 i = 0; i < ToDelete->ConnectedSegments.Num(); i++)
		{
			FLandscapeSplineConnection& Connection = ToDelete->ConnectedSegments[i];
			Connection.Segment->Modify();
			Connection.Segment->DeleteSplinePoints();

			// Get the control point at the *other* end of the segment and remove it from it
			ULandscapeSplineControlPoint* OtherEnd = Connection.GetFarConnection().ControlPoint;
			OtherEnd->Modify();
			OtherEnd->ConnectedSegments.Remove(FLandscapeSplineConnection(Connection.Segment, 1 - Connection.End));
			SplinesComponent->Segments.Remove(Connection.Segment);

			if (bDeleteLooseEnds)
			{
				if (OtherEnd != ToDelete
					&& OtherEnd->ConnectedSegments.Num() == 0)
				{
					SplinesComponent->ControlPoints.Remove(OtherEnd);
				}
			}
		}

		ToDelete->ConnectedSegments.Empty();

		SplinesComponent->ControlPoints.Remove(ToDelete);
		SplinesComponent->MarkRenderStateDirty();
	}

	void SplitSegment(ULandscapeSplineSegment* Segment, const FVector& LocalLocation)
	{
		FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_SplitSegment", "Split Landscape Spline Segment") );

		ULandscapeSplinesComponent* SplinesComponent = Segment->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		Segment->Modify();
		Segment->Connections[1].ControlPoint->Modify();

		float t;
		FVector Location;
		FVector Tangent;
		Segment->FindNearest(LocalLocation, t, Location, Tangent);

		ULandscapeSplineControlPoint* NewControlPoint = ConstructObject<ULandscapeSplineControlPoint>(ULandscapeSplineControlPoint::StaticClass(), SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->ControlPoints.Add(NewControlPoint);

		NewControlPoint->Location = Location;
		NewControlPoint->Rotation = Tangent.Rotation();
		NewControlPoint->Rotation.Roll = FMath::Lerp(Segment->Connections[0].ControlPoint->Rotation.Roll, Segment->Connections[1].ControlPoint->Rotation.Roll, t);
		NewControlPoint->Width = FMath::Lerp(Segment->Connections[0].ControlPoint->Width, Segment->Connections[1].ControlPoint->Width, t);
		NewControlPoint->SideFalloff = FMath::Lerp(Segment->Connections[0].ControlPoint->SideFalloff, Segment->Connections[1].ControlPoint->SideFalloff, t);
		NewControlPoint->EndFalloff = FMath::Lerp(Segment->Connections[0].ControlPoint->EndFalloff, Segment->Connections[1].ControlPoint->EndFalloff, t);

		ULandscapeSplineSegment* NewSegment = ConstructObject<ULandscapeSplineSegment>(ULandscapeSplineSegment::StaticClass(), SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->Segments.Add(NewSegment);

		NewSegment->Connections[0].ControlPoint = NewControlPoint;
		NewSegment->Connections[0].TangentLen = Tangent.Size() * (1 - t);
		NewSegment->Connections[0].ControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 0));
		NewSegment->Connections[1].ControlPoint = Segment->Connections[1].ControlPoint;
		NewSegment->Connections[1].TangentLen = Segment->Connections[1].TangentLen * (1 - t);
		NewSegment->Connections[1].ControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 1));
		NewSegment->LayerName        = Segment->LayerName;
		NewSegment->SplineMeshes     = Segment->SplineMeshes;
		NewSegment->bRaiseTerrain    = Segment->bRaiseTerrain;
		NewSegment->bLowerTerrain    = Segment->bLowerTerrain;
		NewSegment->bEnableCollision = Segment->bEnableCollision;
		NewSegment->bCastShadow      = Segment->bCastShadow;

		Segment->Connections[0].TangentLen *= t;
		Segment->Connections[1].ControlPoint->ConnectedSegments.Remove(FLandscapeSplineConnection(Segment, 1));
		Segment->Connections[1].ControlPoint = NewControlPoint;
		Segment->Connections[1].TangentLen = -Tangent.Size() * t;
		Segment->Connections[1].ControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 1));

		Segment->UpdateSplinePoints();
		NewSegment->UpdateSplinePoints();

		ClearSelection();
		UpdatePropertiesWindows();

		SplinesComponent->MarkRenderStateDirty();
	}

	void FlipSegment(ULandscapeSplineSegment* Segment)
	{
		FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_FlipSegment", "Flip Landscape Spline Segment") );

		ULandscapeSplinesComponent* SplinesComponent = Segment->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		Segment->Modify();

		Segment->Connections[0].ControlPoint->Modify();
		Segment->Connections[1].ControlPoint->Modify();
		Segment->Connections[0].ControlPoint->ConnectedSegments.FindByKey(FLandscapeSplineConnection(Segment, 0))->End = 1;
		Segment->Connections[1].ControlPoint->ConnectedSegments.FindByKey(FLandscapeSplineConnection(Segment, 1))->End = 0;
		Swap(Segment->Connections[0], Segment->Connections[1]);

		Segment->UpdateSplinePoints();
	}

	void SnapControlPointToGround(ULandscapeSplineControlPoint* ControlPoint)
	{
		FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_SnapToGround", "Snap Landscape Spline to Ground") );

		ULandscapeSplinesComponent* SplinesComponent = ControlPoint->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		ControlPoint->Modify();

		const ALandscapeProxy* Landscape = CastChecked<ALandscapeProxy>(SplinesComponent->GetOuter());
		const FTransform LocalToWorld = Landscape->GetTransform();

		const FVector Start = LocalToWorld.TransformPosition(ControlPoint->Location);
		const FVector End = Start + FVector(0, 0, -HALF_WORLD_MAX);

		static FName TraceTag = FName(TEXT("SnapLandscapeSplineControlPointToGround"));
		FHitResult Hit;
		UWorld* World = SplinesComponent->GetWorld();
		check(World);
		if (World->LineTraceSingle(Hit, Start, End, FCollisionQueryParams(true), FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			ControlPoint->Location = LocalToWorld.InverseTransformPosition(Hit.Location);
			ControlPoint->UpdateSplinePoints();
			SplinesComponent->MarkRenderStateDirty();
		}
	}

	void ShowSplineProperties() 
	{
		TArray<UObject*> Objects;
		Objects.Reset(SelectedSplineControlPoints.Num() + SelectedSplineSegments.Num());

		for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
		{
			Objects.Add(*It);
		}
		for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
		{
			Objects.Add(*It);
		}

		FPropertyEditorModule& PropertyModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>( TEXT("PropertyEditor") );
		if( !PropertyModule.HasUnlockedDetailViews() )
		{
			PropertyModule.CreateFloatingDetailsView(Objects, true);
		}
		else
		{
			PropertyModule.UpdatePropertyViews(Objects);
		}
	}

	virtual bool BeginTool( FLevelEditorViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, const FVector& InHitLocation) OVERRIDE
	{
		LandscapeInfo = InTarget.LandscapeInfo.Get();

		ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
		if (!Landscape)
		{
			return true;
		}

		if (!Landscape->SplineComponent)
		{
			CreateSplineComponent(Landscape);
		}

		const FTransform LandscapeToSpline = Landscape->ActorToWorld().GetRelativeTransform(Landscape->SplineComponent->ComponentToWorld);

		AddControlPoint(Landscape, LandscapeToSpline.TransformPosition(InHitLocation));

		GUnrealEd->RedrawLevelEditingViewports();

		return true;
	}

	virtual void EndTool(FLevelEditorViewportClient* ViewportClient) OVERRIDE
	{
		LandscapeInfo = NULL;
	}

	virtual bool MouseMove( FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y ) OVERRIDE
	{
		FVector HitLocation;
		if( EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitLocation)  )
		{
			//if( bToolActive )
			//{
			//	// Apply tool
			//	ApplyTool(ViewportClient);
			//}
		}

		return true;
	}	

	virtual void ApplyTool( FLevelEditorViewportClient* ViewportClient )
	{
	}

	virtual bool HandleClick( HHitProxy* HitProxy, const FViewportClick& Click ) OVERRIDE
	{
		if ( (!HitProxy || !HitProxy->IsA(HWidgetAxis::StaticGetType()))
			&& !Click.IsShiftDown())
		{
			ClearSelection();
			UpdatePropertiesWindows();
			GUnrealEd->RedrawLevelEditingViewports();
		}

		if (HitProxy)
		{
			ULandscapeSplineControlPoint* ClickedControlPoint = NULL;
			ULandscapeSplineSegment* ClickedSplineSegment = NULL;

			if (HitProxy->IsA(HLandscapeSplineProxy_ControlPoint::StaticGetType()) )
			{
				HLandscapeSplineProxy_ControlPoint* SplineProxy = (HLandscapeSplineProxy_ControlPoint*)HitProxy;
				ClickedControlPoint = SplineProxy->ControlPoint;
			}
			else if (HitProxy->IsA(HLandscapeSplineProxy_Segment::StaticGetType()) )
			{
				HLandscapeSplineProxy_Segment* SplineProxy = (HLandscapeSplineProxy_Segment*)HitProxy;
				ClickedSplineSegment = SplineProxy->SplineSegment;
			}
			else if (HitProxy->IsA(HActor::StaticGetType()) )
			{
				HActor* ActorProxy = (HActor*)HitProxy;
				ULandscapeSplinesComponent* SplineComponent = ActorProxy->Actor->FindComponentByClass<ULandscapeSplinesComponent>();
				if (SplineComponent != NULL)
				{
					const UControlPointMeshComponent* ControlPointMeshComponent = Cast<const UControlPointMeshComponent>(ActorProxy->PrimComponent);
					const USplineMeshComponent* SplineMeshComponent = Cast<const USplineMeshComponent>(ActorProxy->PrimComponent);
					if (ControlPointMeshComponent != NULL)
					{
						for (auto It = SplineComponent->ControlPoints.CreateConstIterator(); It; ++It)
						{
							ULandscapeSplineControlPoint* ControlPoint = *It;
							if (ControlPoint->OwnsComponent(ControlPointMeshComponent))
							{
								ClickedControlPoint = ControlPoint;
								break;
							}
						}
					}
					else if (SplineMeshComponent != NULL)
					{
						for (auto It = SplineComponent->Segments.CreateConstIterator(); It; ++It)
						{
							ULandscapeSplineSegment* SplineSegment = *It;
							if (SplineSegment->OwnsComponent(SplineMeshComponent))
							{
								ClickedSplineSegment = SplineSegment;
								break;
							}
						}
					}
				}
			}

			if (ClickedControlPoint != NULL)
			{
				if (Click.IsShiftDown() && ClickedControlPoint->IsSplineSelected())
				{
					DeselectControlPoint(ClickedControlPoint);
				}
				else
				{
					SelectControlPoint(ClickedControlPoint);
				}
				GEditor->SelectNone(true, true);
				UpdatePropertiesWindows();

				GUnrealEd->RedrawLevelEditingViewports();
				return true;
			}
			else if (ClickedSplineSegment != NULL)
			{
				// save info about what we grabbed
				if (Click.IsShiftDown() && ClickedSplineSegment->IsSplineSelected())
				{
					DeSelectSegment(ClickedSplineSegment);
				}
				else
				{
					SelectSegment(ClickedSplineSegment);
				}
				GEditor->SelectNone(true, true);
				UpdatePropertiesWindows();

				GUnrealEd->RedrawLevelEditingViewports();
				return true;
			}
		}

		return false;
	}

	virtual bool InputKey( FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent ) OVERRIDE
	{
		if (InKey == EKeys::F4 && InEvent == IE_Pressed)
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				ShowSplineProperties();
				return true;
			}
		}

		if (InKey == EKeys::R && InEvent == IE_Pressed)
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_AutoRotate", "Auto-rotate Landscape Spline Control Points") );

				for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
				{
					(*It)->AutoCalcRotation();
					(*It)->UpdateSplinePoints();
				}

				for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
				{
					(*It)->Connections[0].ControlPoint->AutoCalcRotation();
					(*It)->Connections[0].ControlPoint->UpdateSplinePoints();
					(*It)->Connections[1].ControlPoint->AutoCalcRotation();
					(*It)->Connections[1].ControlPoint->UpdateSplinePoints();
				}

				return true;
			}
		}

		if (InKey == EKeys::F && InEvent == IE_Pressed)
		{
			if (SelectedSplineSegments.Num() > 0)
			{
				FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_FlipSegments", "Flip Landscape Spline Segments") );

				for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
				{
					FlipSegment(*It);
				}

				return true;
			}
		}

		if (InKey == EKeys::T && InEvent == IE_Pressed)
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_AutoFlipTangents", "Auto-flip Landscape Spline Tangents") );

				for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
				{
					(*It)->AutoFlipTangents();
					(*It)->UpdateSplinePoints();
				}

				for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
				{
					(*It)->Connections[0].ControlPoint->AutoFlipTangents();
					(*It)->Connections[0].ControlPoint->UpdateSplinePoints();
					(*It)->Connections[1].ControlPoint->AutoFlipTangents();
					(*It)->Connections[1].ControlPoint->UpdateSplinePoints();
				}

				return true;
			}
		}

		if (InKey == EKeys::End && InEvent == IE_Pressed)
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_SnapToGround", "Snap Landscape Spline to Ground") );

				for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
				{
					SnapControlPointToGround((*It));
				}
				for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
				{
					SnapControlPointToGround((*It)->Connections[0].ControlPoint);
					SnapControlPointToGround((*It)->Connections[1].ControlPoint);
				}
				UpdatePropertiesWindows();

				GUnrealEd->RedrawLevelEditingViewports();
				return true;
			}
		}

		if (InKey == EKeys::A && InEvent == IE_Pressed
			&& IsCtrlDown(InViewport))
		{
			if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
			{
				SelectConnected();

				UpdatePropertiesWindows();

				GUnrealEd->RedrawLevelEditingViewports();
				return true;
			}
		}

		if (SelectedSplineControlPoints.Num() > 0)
		{
			if (InKey == EKeys::LeftMouseButton && InEvent == IE_Pressed
				&& IsCtrlDown(InViewport))
			{
				int32 HitX = InViewport->GetMouseX();
				int32 HitY = InViewport->GetMouseY();
				HHitProxy*	HitProxy = InViewport->GetHitProxy(HitX, HitY);
				if (HitProxy != NULL)
				{
					ULandscapeSplineControlPoint* ClickedControlPoint = NULL;

					if (HitProxy->IsA(HLandscapeSplineProxy_ControlPoint::StaticGetType()) )
					{
						HLandscapeSplineProxy_ControlPoint* SplineProxy = (HLandscapeSplineProxy_ControlPoint*)HitProxy;
						ClickedControlPoint = SplineProxy->ControlPoint;
					}
					else if (HitProxy->IsA(HActor::StaticGetType()) )
					{
						HActor* ActorProxy = (HActor*)HitProxy;
						ULandscapeSplinesComponent* SplineComponent = ActorProxy->Actor->FindComponentByClass<ULandscapeSplinesComponent>();
						if (SplineComponent != NULL)
						{
							const UControlPointMeshComponent* ControlPointMeshComponent = Cast<const UControlPointMeshComponent>(ActorProxy->PrimComponent);
							if (ControlPointMeshComponent != NULL)
							{
								for (auto It = SplineComponent->ControlPoints.CreateConstIterator(); It; ++It)
								{
									ULandscapeSplineControlPoint* ControlPoint = *It;
									if (ControlPoint->OwnsComponent(ControlPointMeshComponent))
									{
										ClickedControlPoint = ControlPoint;
										break;
									}
								}
							}
						}
					}

					if (ClickedControlPoint != NULL)
					{
						FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_AddSegment", "Add Landscape Spline Segment") );

						for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
						{
							AddSegment((*It), ClickedControlPoint, bAutoRotateOnJoin, bAutoRotateOnJoin);
						}

						GUnrealEd->RedrawLevelEditingViewports();

						return true;
					}
				}
			}
		}

		if (SelectedSplineControlPoints.Num() == 0)
		{
			if (InKey == EKeys::LeftMouseButton && InEvent == IE_Pressed
				&& IsCtrlDown(InViewport))
			{
				int32 HitX = InViewport->GetMouseX();
				int32 HitY = InViewport->GetMouseY();
				HHitProxy* HitProxy = InViewport->GetHitProxy(HitX, HitY);
				if (HitProxy)
				{
					ULandscapeSplineSegment* ClickedSplineSegment = NULL;
					FTransform LandscapeToSpline;

					if (HitProxy->IsA(HLandscapeSplineProxy_Segment::StaticGetType()) )
					{
						HLandscapeSplineProxy_Segment* SplineProxy = (HLandscapeSplineProxy_Segment*)HitProxy;
						ClickedSplineSegment = SplineProxy->SplineSegment;

						LandscapeToSpline = ClickedSplineSegment->GetTypedOuter<AActor>()->ActorToWorld().GetRelativeTransform(ClickedSplineSegment->GetTypedOuter<ULandscapeSplinesComponent>()->ComponentToWorld);
					}
					else if (HitProxy->IsA(HActor::StaticGetType()) )
					{
						HActor* ActorProxy = (HActor*)HitProxy;
						const USplineMeshComponent* SplineMeshComponent = Cast<const USplineMeshComponent>(ActorProxy->PrimComponent);
						if (SplineMeshComponent != NULL)
						{
							ULandscapeSplinesComponent* SplineComponent = ActorProxy->Actor->FindComponentByClass<ULandscapeSplinesComponent>();
							if (SplineComponent != NULL)
							{
								for (auto It = SplineComponent->Segments.CreateConstIterator(); It; ++It)
								{
									ULandscapeSplineSegment* SplineSegment = *It;
									if (SplineSegment->OwnsComponent(SplineMeshComponent))
									{
										ClickedSplineSegment = SplineSegment;
										LandscapeToSpline = ActorProxy->Actor->ActorToWorld().GetRelativeTransform(SplineComponent->ComponentToWorld);
										break;
									}
								}
							}
						}
					}

					if (ClickedSplineSegment != NULL)
					{
						FVector HitLocation;
						if( EdMode->LandscapeMouseTrace(InViewportClient, HitLocation) )
						{
							FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_SplitSegment", "Split Landscape Spline Segment") );

							SplitSegment(ClickedSplineSegment, LandscapeToSpline.TransformPosition(HitLocation));

							GUnrealEd->RedrawLevelEditingViewports();
						}

						return true;
					}
				}
			}
		}

		if (InKey == EKeys::LeftMouseButton)
		{
			// Press mouse button
			if (InEvent == IE_Pressed)
			{
				// See if we clicked on a spline handle..
				int32 HitX = InViewport->GetMouseX();
				int32 HitY = InViewport->GetMouseY();
				HHitProxy*	HitProxy = InViewport->GetHitProxy(HitX, HitY);
				if (HitProxy)
				{
					if (HitProxy->IsA(HWidgetAxis::StaticGetType()) )
					{
						checkSlow(SelectedSplineControlPoints.Num() > 0);
						bMovingControlPoint = true;

						GEditor->BeginTransaction( LOCTEXT("LandscapeSpline_ModifyControlPoint", "Modify Landscape Spline Control Point") );
						for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
						{
							(*It)->Modify();
							(*It)->GetOuterULandscapeSplinesComponent()->Modify();
						}

						return false; // We're not actually handling this case ourselves, just wrapping it in a transaction
					}
					else if (HitProxy->IsA(HLandscapeSplineProxy_Tangent::StaticGetType()) )
					{
						HLandscapeSplineProxy_Tangent* SplineProxy = (HLandscapeSplineProxy_Tangent*)HitProxy;
						DraggingTangent_Segment = SplineProxy->SplineSegment;
						DraggingTangent_End = SplineProxy->End;

						GEditor->BeginTransaction( LOCTEXT("LandscapeSpline_ModifyTangent", "Modify Landscape Spline Tangent") );
						ULandscapeSplinesComponent* SplinesComponent = DraggingTangent_Segment->GetOuterULandscapeSplinesComponent();
						SplinesComponent->Modify();
						DraggingTangent_Segment->Modify();

						return false; // false to let FLevelEditorViewportClient.InputKey start mouse tracking and enable InputDelta() so we can use it
					}
				}
			}
			else if (InEvent == IE_Released)
			{
				if (bMovingControlPoint)
				{
					bMovingControlPoint = false;

					for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
					{
						(*It)->UpdateSplinePoints(true);
					}

					GEditor->EndTransaction();

					return false; // We're not actually handling this case ourselves, just wrapping it in a transaction
				}
				else if (DraggingTangent_Segment)
				{
					DraggingTangent_Segment->UpdateSplinePoints(true);

					DraggingTangent_Segment = NULL;

					GEditor->EndTransaction();

					return false; // false to let FLevelEditorViewportClient.InputKey end mouse tracking
				}
			}
		}

		return false;
	}

	virtual bool InputDelta( FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale ) OVERRIDE
	{
		FVector Drag = InDrag;

		if (DraggingTangent_Segment)
		{
			const ULandscapeSplinesComponent* SplinesComponent = DraggingTangent_Segment->GetOuterULandscapeSplinesComponent();
			FLandscapeSplineSegmentConnection& Connection = DraggingTangent_Segment->Connections[DraggingTangent_End];

			FVector StartLocation; FRotator StartRotation;
			Connection.ControlPoint->GetConnectionLocationAndRotation(Connection.SocketName, StartLocation, StartRotation);

			float OldTangentLen = Connection.TangentLen;
			Connection.TangentLen += SplinesComponent->ComponentToWorld.InverseTransformVector(-Drag) | StartRotation.Vector();

			// Disallow a tangent of exactly 0
			if (Connection.TangentLen == 0)
			{
				if (OldTangentLen > 0)
				{
					Connection.TangentLen = SMALL_NUMBER;
				}
				else
				{
					Connection.TangentLen = -SMALL_NUMBER;
				}
			}

			// Flipping the tangent is only allowed if not using a socket
			if (Connection.SocketName != NAME_None)
			{
				Connection.TangentLen = FMath::Max(SMALL_NUMBER, Connection.TangentLen);
			}

			DraggingTangent_Segment->UpdateSplinePoints(false);

			return true;
		}

		if (SelectedSplineControlPoints.Num() > 0 && InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
		{
			for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
			{
				ULandscapeSplineControlPoint* ControlPoint = (*It);
				const ULandscapeSplinesComponent* SplinesComponent = ControlPoint->GetOuterULandscapeSplinesComponent();

				ControlPoint->Location += SplinesComponent->ComponentToWorld.InverseTransformVector(Drag);

				FVector RotAxis; float RotAngle;
				InRot.Quaternion().ToAxisAndAngle(RotAxis, RotAngle);
				RotAxis = (SplinesComponent->ComponentToWorld.GetRotation().Inverse() * ControlPoint->Rotation.Quaternion().Inverse()).RotateVector(RotAxis);

				// Hack: for some reason FQuat.Rotator() Clamps to 0-360 range, so use .GetNormalized() to recover the original negative rotation.
				ControlPoint->Rotation += FQuat(RotAxis, RotAngle).Rotator().GetNormalized();

				ControlPoint->Rotation.Yaw = FRotator::NormalizeAxis(ControlPoint->Rotation.Yaw);
				ControlPoint->Rotation.Pitch = FMath::Clamp(ControlPoint->Rotation.Pitch, -85.0f, 85.0f);
				ControlPoint->Rotation.Roll = FMath::Clamp(ControlPoint->Rotation.Roll, -85.0f, 85.0f);

				if (bAutoChangeConnectionsOnMove)
				{
					ControlPoint->AutoSetConnections(true);
				}

				ControlPoint->UpdateSplinePoints(false);
			}

			return true;
		}

		return false;
	}

	void FixSelection()
	{
		SelectedSplineControlPoints.Empty();
		SelectedSplineSegments.Empty();

		if (EdMode->CurrentToolSet != NULL && EdMode->CurrentToolSet->GetTool() == this)
		{
			for (auto It = EdMode->GetLandscapeList().CreateConstIterator(); It; ++It)
			{
				ULandscapeInfo* LandscapeInfo = (*It).Info;

				ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
				if (Landscape != NULL && Landscape->SplineComponent != NULL)
				{
					for (auto It = Landscape->SplineComponent->ControlPoints.CreateConstIterator(); It; ++It)
					{
						if ((*It)->IsSplineSelected())
						{
							SelectedSplineControlPoints.Add(*It);
						}
					}

					for (auto It = Landscape->SplineComponent->Segments.CreateConstIterator(); It; ++It)
					{
						if ((*It)->IsSplineSelected())
						{
							SelectedSplineSegments.Add(*It);
						}
					}
				}
			}
		}
		else
		{
			for (auto It = EdMode->GetLandscapeList().CreateConstIterator(); It; ++It)
			{
				ULandscapeInfo* LandscapeInfo = (*It).Info;

				ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
				if (Landscape != NULL && Landscape->SplineComponent != NULL)
				{
					for (auto It = Landscape->SplineComponent->ControlPoints.CreateConstIterator(); It; ++It)
					{
						(*It)->SetSplineSelected(false);
					}

					for (auto It = Landscape->SplineComponent->Segments.CreateConstIterator(); It; ++It)
					{
						(*It)->SetSplineSelected(false);
					}
				}
			}
		}
	}

	void OnUndo()
	{
		FixSelection();
		UpdatePropertiesWindows();
	}

	virtual void EnterTool() OVERRIDE
	{
		GEditor->SelectNone(true, true, false);

		for (auto It = EdMode->GetLandscapeList().CreateConstIterator(); It; ++It)
		{
			ULandscapeInfo* LandscapeInfo = (*It).Info;

			ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
			if (Landscape != NULL && Landscape->SplineComponent != NULL)
			{
				Landscape->SplineComponent->ShowSplineEditorMesh(true);
			}
		}
	}

	virtual void ExitTool() OVERRIDE
	{
		ClearSelection();
		UpdatePropertiesWindows();

		for (auto It = EdMode->GetLandscapeList().CreateConstIterator(); It; ++It)
		{
			ULandscapeInfo* LandscapeInfo = (*It).Info;

			ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
			if (Landscape != NULL && Landscape->SplineComponent != NULL)
			{
				Landscape->SplineComponent->ShowSplineEditorMesh(false);
			}
		}
	}

	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) OVERRIDE
	{
		if (SelectedSplineControlPoints.Num() > 0)
		{
			for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
			{
				const ULandscapeSplineControlPoint* ControlPoint = (*It);
				const ULandscapeSplinesComponent* SplinesComponent = ControlPoint->GetOuterULandscapeSplinesComponent();

				FVector HandlePos0 = SplinesComponent->ComponentToWorld.TransformPosition(ControlPoint->Location + ControlPoint->Rotation.Vector() * -20);
				FVector HandlePos1 = SplinesComponent->ComponentToWorld.TransformPosition(ControlPoint->Location + ControlPoint->Rotation.Vector() * 20);
				DrawDashedLine(PDI, HandlePos0, HandlePos1, FColor::White, 20, SDPG_Foreground);

				if (GEditorModeTools().GetWidgetMode() == FWidget::WM_Scale)
				{
					for (int32 i = 0; i < ControlPoint->ConnectedSegments.Num(); i++)
					{
						const FLandscapeSplineConnection& Connection = ControlPoint->ConnectedSegments[i];

						FVector StartLocation; FRotator StartRotation;
						Connection.GetNearConnection().ControlPoint->GetConnectionLocationAndRotation(Connection.GetNearConnection().SocketName, StartLocation, StartRotation);

						FVector StartPos = SplinesComponent->ComponentToWorld.TransformPosition(StartLocation);
						FVector HandlePos = SplinesComponent->ComponentToWorld.TransformPosition(StartLocation + StartRotation.Vector() * Connection.GetNearConnection().TangentLen / 2);
						PDI->DrawLine(StartPos, HandlePos, FColor::White, SDPG_Foreground);

						if (PDI->IsHitTesting()) PDI->SetHitProxy( new HLandscapeSplineProxy_Tangent(Connection.Segment, Connection.End) );
						PDI->DrawPoint(HandlePos, FColor(255,255,255), 10.f, SDPG_Foreground);
						if (PDI->IsHitTesting()) PDI->SetHitProxy( NULL );
					}
				}
			}
		}

		if (SelectedSplineSegments.Num() > 0)
		{
			if (GEditorModeTools().GetWidgetMode() == FWidget::WM_Scale)
			{
				for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
				{
					const ULandscapeSplineSegment* Segment = (*It);
					const ULandscapeSplinesComponent* SplinesComponent = Segment->GetOuterULandscapeSplinesComponent();
					for (int32 End = 0; End <= 1; End++)
					{
						const FLandscapeSplineSegmentConnection& Connection = Segment->Connections[End];

						FVector StartLocation; FRotator StartRotation;
						Connection.ControlPoint->GetConnectionLocationAndRotation(Connection.SocketName, StartLocation, StartRotation);

						FVector EndPos = SplinesComponent->ComponentToWorld.TransformPosition(StartLocation);
						FVector EndHandlePos = SplinesComponent->ComponentToWorld.TransformPosition(StartLocation + StartRotation.Vector() * Connection.TangentLen / 2);

						PDI->DrawLine(EndPos, EndHandlePos, FColor::White, SDPG_Foreground);
						if (PDI->IsHitTesting()) PDI->SetHitProxy( new HLandscapeSplineProxy_Tangent((*It), !!End) );
						PDI->DrawPoint(EndHandlePos, FColor(255,255,255), 10.f, SDPG_Foreground);
						if (PDI->IsHitTesting()) PDI->SetHitProxy( NULL );
					}
				}
			}
		}
	}

	virtual bool OverrideSelection() const OVERRIDE
	{
		return true;
	}

	virtual bool IsSelectionAllowed( AActor* InActor, bool bInSelection ) const OVERRIDE
	{
		// Only filter selection not deselection
		if (bInSelection)
		{
			return false;
		}

		return true;
	}

	virtual bool UsesTransformWidget() const OVERRIDE
	{
		if (SelectedSplineControlPoints.Num() > 0)
		{
			return true;
		}

		return false;
	}

	virtual EAxisList::Type GetWidgetAxisToDraw(FWidget::EWidgetMode CheckMode) const OVERRIDE
	{
		if (SelectedSplineControlPoints.Num() > 0)
		{
			//if (CheckMode == FWidget::WM_Rotate
			//	&& SelectedSplineControlPoints.Num() >= 2)
			//{
			//	return AXIS_X;
			//}
			//else
			if (CheckMode != FWidget::WM_Scale)
			{
				return EAxisList::XYZ;
			}
			else
			{
				return EAxisList::None;
			}
		}

		return EAxisList::None;
	}

	virtual FVector GetWidgetLocation() const OVERRIDE
	{
		if (SelectedSplineControlPoints.Num() > 0)
		{
			ULandscapeSplineControlPoint* FirstPoint = *SelectedSplineControlPoints.CreateConstIterator();
			ULandscapeSplinesComponent* SplinesComponent = FirstPoint->GetOuterULandscapeSplinesComponent();
			return SplinesComponent->ComponentToWorld.TransformPosition(FirstPoint->Location);
		}

		return FVector::ZeroVector;
	}

	virtual FMatrix GetWidgetRotation() const OVERRIDE
	{
		if (SelectedSplineControlPoints.Num() > 0)
		{
			ULandscapeSplineControlPoint* FirstPoint = *SelectedSplineControlPoints.CreateConstIterator();
			ULandscapeSplinesComponent* SplinesComponent = FirstPoint->GetOuterULandscapeSplinesComponent();
			return FQuatRotationTranslationMatrix(FirstPoint->Rotation.Quaternion() * SplinesComponent->ComponentToWorld.GetRotation(), FVector::ZeroVector);
		}

		return FMatrix::Identity;
	}

	virtual EEditAction::Type GetActionEditDuplicate() OVERRIDE
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual EEditAction::Type GetActionEditDelete() OVERRIDE
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual EEditAction::Type GetActionEditCut() OVERRIDE
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual EEditAction::Type GetActionEditCopy() OVERRIDE
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual EEditAction::Type GetActionEditPaste() OVERRIDE
	{
		FString PasteString;
		FPlatformMisc::ClipboardPaste(PasteString);
		if (PasteString.StartsWith("BEGIN SPLINES"))
		{
			return EEditAction::Process;
		}

		return EEditAction::Skip;
	}

	virtual bool ProcessEditDuplicate() OVERRIDE
	{
		InternalProcessEditDuplicate();
		return true;
	}

	virtual bool ProcessEditDelete() OVERRIDE
	{
		InternalProcessEditDelete();
		return true;
	}

	virtual bool ProcessEditCut() OVERRIDE
	{
		InternalProcessEditCut();
		return true;
	}

	virtual bool ProcessEditCopy() OVERRIDE
	{
		InternalProcessEditCopy();
		return true;
	}

	virtual bool ProcessEditPaste() OVERRIDE
	{
		InternalProcessEditPaste();
		return true;
	}

	void InternalProcessEditDuplicate()
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_Duplicate", "Duplicate Landscape Splines") );

			FString Data;
			InternalProcessEditCopy(&Data);
			InternalProcessEditPaste(&Data, true);
		}
	}

	void InternalProcessEditDelete()
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_Delete", "Delete Landscape Splines") );

			for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
			{
				DeleteControlPoint((*It), bDeleteLooseEnds);
			}
			for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
			{
				DeleteSegment((*It), bDeleteLooseEnds);
			}
			ClearSelection();
			UpdatePropertiesWindows();

			GUnrealEd->RedrawLevelEditingViewports();
		}
	}

	void InternalProcessEditCut()
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_Cut", "Cut Landscape Splines") );

			InternalProcessEditCopy();
			InternalProcessEditDelete();
		}
	}

	void InternalProcessEditCopy(FString* OutData = NULL)
	{
		if (SelectedSplineControlPoints.Num() > 0 || SelectedSplineSegments.Num() > 0)
		{
			TArray<UObject*> Objects;
			Objects.Reserve(SelectedSplineControlPoints.Num() + SelectedSplineSegments.Num() * 3); // worst case

			// Control Points then segments
			for (auto It = SelectedSplineControlPoints.CreateConstIterator(); It; ++It)
			{
				ULandscapeSplineControlPoint* ControlPoint = *It;
				Objects.Add(ControlPoint);
			}
			for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
			{
				ULandscapeSplineSegment* Segment = *It;
				Objects.AddUnique(Segment->Connections[0].ControlPoint);
				Objects.AddUnique(Segment->Connections[1].ControlPoint);
			}
			for (auto It = SelectedSplineSegments.CreateConstIterator(); It; ++It)
			{
				ULandscapeSplineSegment* Segment = *It;
				Objects.Add(Segment);
			}

			// Perform export to text format
			FStringOutputDevice Ar;
			Ar.Logf(TEXT("Begin Splines\r\n"));
			for (auto It = Objects.CreateConstIterator(); It; ++It)
			{
				UObject* Object = *It;
				UExporter::ExportToOutputDevice(NULL, Object, NULL, Ar, TEXT("copy"), 3, PPF_None, false);
			}
			Ar.Logf(TEXT("End Splines\r\n"));

			if (OutData != NULL)
			{
				*OutData = MoveTemp(Ar);
			}
			else
			{
				FPlatformMisc::ClipboardCopy(*Ar);
			}
		}
	}

	void InternalProcessEditPaste(FString* InData = NULL, bool bOffset = false)
	{
		FScopedTransaction Transaction( LOCTEXT("LandscapeSpline_Paste", "Paste Landscape Splines") );

		ALandscape* Landscape = EdMode->CurrentToolTarget.LandscapeInfo->LandscapeActor.Get();
		if (!Landscape)
		{
			return;
		}
		if (!Landscape->SplineComponent)
		{
			CreateSplineComponent(Landscape);
		}

		const TCHAR* Data = NULL;
		FString PasteString;
		if (InData != NULL)
		{
			Data = **InData;
		}
		else
		{
			FPlatformMisc::ClipboardPaste(PasteString);
			Data = *PasteString;
		}

		FLandscapeSplineTextObjectFactory Factory;
		TArray<UObject*> OutObjects = Factory.ImportSplines(Landscape->SplineComponent, Data);

		if (bOffset)
		{
			for (auto It = OutObjects.CreateIterator(); It; ++It)
			{
				ULandscapeSplineControlPoint* ControlPoint = Cast<ULandscapeSplineControlPoint>(*It);
				if (ControlPoint != NULL)
				{
					Landscape->SplineComponent->ControlPoints.Add(ControlPoint);
					ControlPoint->Location += FVector(500, 500, 0);

					ControlPoint->UpdateSplinePoints();
				}
			}
		}
	}

protected:
	// Begin FEditorUndoClient
	virtual void PostUndo(bool bSuccess) OVERRIDE { OnUndo(); }
	virtual void PostRedo(bool bSuccess) OVERRIDE { PostUndo(bSuccess); }
	// End of FEditorUndoClient

protected:
	class FEdModeLandscape* EdMode;
	class ULandscapeInfo* LandscapeInfo;

	TSet<ULandscapeSplineControlPoint*> SelectedSplineControlPoints;
	TSet<ULandscapeSplineSegment*> SelectedSplineSegments;

	ULandscapeSplineSegment* DraggingTangent_Segment;
	uint32 DraggingTangent_End:1;

	uint32 bMovingControlPoint:1;

	uint32 bAutoRotateOnJoin:1;
	uint32 bAutoChangeConnectionsOnMove:1;
	uint32 bDeleteLooseEnds:1;
	uint32 bCopyMeshToNewControlPoint:1;
};


void FEdModeLandscape::ShowSplineProperties()
{
	if (SplinesToolSet /*&& SplinesToolSet == CurrentToolSet*/ )
	{
		if (SplinesToolSet->SetToolForTarget(CurrentToolTarget) && SplinesToolSet->GetTool())
		{
			((FLandscapeToolSplines*)SplinesToolSet->GetTool())->ShowSplineProperties();
		}
	}
}

void FEdModeLandscape::SelectAllConnectedSplineControlPoints()
{
	if (SplinesToolSet /*&& SplinesToolSet == CurrentToolSet*/ )
	{
		if (SplinesToolSet->SetToolForTarget(CurrentToolTarget) && SplinesToolSet->GetTool())
		{
			FLandscapeToolSplines* SplineTool = ((FLandscapeToolSplines*)SplinesToolSet->GetTool());
			SplineTool->SelectAdjacentControlPoints();
			SplineTool->ClearSelectedSegments();
			SplineTool->SelectConnected();

			SplineTool->UpdatePropertiesWindows();
			GUnrealEd->RedrawLevelEditingViewports();
		}
	}
}

void FEdModeLandscape::SelectAllConnectedSplineSegments()
{
	if (SplinesToolSet /*&& SplinesToolSet == CurrentToolSet*/ )
	{
		if (SplinesToolSet->SetToolForTarget(CurrentToolTarget) && SplinesToolSet->GetTool())
		{
			FLandscapeToolSplines* SplineTool = ((FLandscapeToolSplines*)SplinesToolSet->GetTool());
			SplineTool->SelectAdjacentSegments();
			SplineTool->ClearSelectedControlPoints();
			SplineTool->SelectConnected();

			SplineTool->UpdatePropertiesWindows();
			GUnrealEd->RedrawLevelEditingViewports();
		}
	}
}

void FEdModeLandscape::IntializeToolSet_Splines()
{
	FLandscapeToolSet* ToolSet_Splines = new(LandscapeToolSets) FLandscapeToolSet(TEXT("ToolSet_Splines"));
	ToolSet_Splines->AddTool(new FLandscapeToolSplines(this));
	SplinesToolSet = ToolSet_Splines;

	ToolSet_Splines->ValidBrushes.Add("BrushSet_Splines");
}

#undef LOCTEXT_NAMESPACE
