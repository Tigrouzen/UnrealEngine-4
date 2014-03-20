// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "LayersPrivatePCH.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "Layer"


FActorLayerViewModel::FActorLayerViewModel( const TWeakObjectPtr< ULayer >& InLayer, const TArray< TWeakObjectPtr< AActor > >& InActors, const TSharedRef< ILayers >& InWorldLayers, const TWeakObjectPtr< UEditorEngine >& InEditor )
	: WorldLayers( InWorldLayers )
	, Layer( InLayer )
	, Editor( InEditor )
{
	Actors.Append( InActors );
}


void FActorLayerViewModel::Initialize()
{
	WorldLayers->OnLayersChanged().AddSP( this, &FActorLayerViewModel::OnLayersChanged );

	if ( Editor.IsValid() )
	{
		Editor->RegisterForUndo(this);
	}
}


FActorLayerViewModel::~FActorLayerViewModel()
{
	WorldLayers->OnLayersChanged().RemoveAll( this );

	if ( Editor.IsValid() )
	{
		Editor->UnregisterForUndo(this);
	}
}


FName FActorLayerViewModel::GetFName() const
{
	if( !Layer.IsValid() )
	{
		return NAME_None;
	}

	return Layer->LayerName;
}


FString FActorLayerViewModel::GetName() const
{
	if( !Layer.IsValid() )
	{
		return LOCTEXT("Invalid layer Name", "").ToString();
	}

	return Layer->LayerName.ToString();
}


bool FActorLayerViewModel::IsVisible() const
{
	if( !Layer.IsValid() )
	{
		return false;
	}

	return Layer->bIsVisible;
}


void FActorLayerViewModel::OnLayersChanged( const ELayersAction::Type Action, const TWeakObjectPtr< ULayer >& ChangedLayer, const FName& ChangedProperty )
{
	if( Action != ELayersAction::Modify && Action != ELayersAction::Reset )
	{
		return;
	}

	if( ChangedLayer.IsValid() && ChangedLayer != Layer )
	{
		return;
	}

	Changed.Broadcast();
}


void FActorLayerViewModel::Refresh()
{
	OnLayersChanged( ELayersAction::Reset, NULL, NAME_None );
}


#undef LOCTEXT_NAMESPACE