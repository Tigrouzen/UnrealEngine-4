// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"

#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "GenericEditor"

const FName FSimpleAssetEditor::PropertiesTabId( TEXT( "GenericEditor_Properties" ) );

void FSimpleAssetEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(TabManager);

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	TabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FSimpleAssetEditor::SpawnPropertiesTab) )
		.SetDisplayName( LOCTEXT("PropertiesTab", "Details") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );
}

void FSimpleAssetEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManager);

	TabManager->UnregisterTabSpawner( PropertiesTabId );
}

const FName FSimpleAssetEditor::SimpleEditorAppIdentifier( TEXT( "GenericEditorApp" ) );

FSimpleAssetEditor::~FSimpleAssetEditor()
{
	DetailsView.Reset();
	PropertiesTab.Reset();
}


void FSimpleAssetEditor::InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit )
{
	const bool bIsUpdatable = false;
	const bool bAllowFavorites = true;
	const bool bIsLockable = false;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	const FDetailsViewArgs DetailsViewArgs( bIsUpdatable, bIsLockable, true, false, false );
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_SimpleAssetEditor_Layout_v3" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell( true )
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->Split
			(
				FTabManager::NewStack()
				->AddTab( PropertiesTabId, ETabState::OpenedTab )
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FSimpleAssetEditor::SimpleEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit );

	// @todo toolkit world centric editing
	// Setup our tool's layout
	/*if( IsWorldCentricAssetEditor() && !PropertiesTab.IsValid() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for asset properties
		SpawnToolkitTab(GetToolbarTabId(), FString(), EToolkitTabSpot::ToolBar);
		PropertiesTab = SpawnToolkitTab( PropertiesTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/
	
	// Ensure all objects are transactable for undo/redo in the details panel
	for( int32 ObjectIndex = 0; ObjectIndex < ObjectsToEdit.Num(); ++ObjectIndex )
	{
		ObjectsToEdit[ObjectIndex]->SetFlags(RF_Transactional);
	}

	if( DetailsView.IsValid() )
	{
		// Make sure details window is pointing to our object
		DetailsView->SetObjects( ObjectsToEdit );
	}
}

FName FSimpleAssetEditor::GetToolkitFName() const
{
	return FName("GenericAssetEditor");
}

FText FSimpleAssetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Generic Asset Editor");
}

FText FSimpleAssetEditor::GetToolkitName() const
{
	const auto EditingObjects = GetEditingObjects();

	check( EditingObjects.Num() > 0 );

	FFormatNamedArguments Args;
	Args.Add( TEXT("ToolkitName"), GetBaseToolkitName() );

	if( EditingObjects.Num() == 1 )
	{
		const UObject* EditingObject = EditingObjects[ 0 ];

		const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

		Args.Add( TEXT("ObjectName"), FText::FromString( EditingObject->GetName() ) );
		Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
		return FText::Format( LOCTEXT("ToolkitTitle", "{ObjectName}{DirtyState} - {ToolkitName}"), Args );
	}
	else
	{
		bool bDirtyState = false;
		UClass* SharedBaseClass = NULL;
		for( int32 x = 0; x < EditingObjects.Num(); ++x )
		{
			UObject* Obj = EditingObjects[ x ];
			check( Obj );

			UClass* ObjClass = Cast<UClass>(Obj);
			if (ObjClass == NULL)
			{
				ObjClass = Obj->GetClass();
			}
			check( ObjClass );

			// Initialize with the class of the first object we encounter.
			if( SharedBaseClass == NULL )
			{
				SharedBaseClass = ObjClass;
			}

			// If we've encountered an object that's not a subclass of the current best baseclass,
			// climb up a step in the class hierarchy.
			while( !ObjClass->IsChildOf( SharedBaseClass ) )
			{
				SharedBaseClass = SharedBaseClass->GetSuperClass();
			}

			// If any of the objects are dirty, flag the label
			bDirtyState |= Obj->GetOutermost()->IsDirty();
		}

		Args.Add( TEXT("NumberOfObjects"), EditingObjects.Num() );
		Args.Add( TEXT("ClassName"), FText::FromString( SharedBaseClass->GetName() ) );
		Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
		return FText::Format( LOCTEXT("ToolkitTitle_EditingMultiple", "{NumberOfObjects} {ClassName}{DirtyState} - {ToolkitName}"), Args );
	}
}


FLinearColor FSimpleAssetEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.0f, 0.0f, 0.5f );
}

void FSimpleAssetEditor::SetPropertyVisibilityDelegate(FIsPropertyVisible InVisibilityDelegate)
{
	DetailsView->SetIsPropertyVisibleDelegate(InVisibilityDelegate);
}

TSharedRef<SDockTab> FSimpleAssetEditor::SpawnPropertiesTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == PropertiesTabId );

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("GenericEditor.Tabs.Properties") )
		.Label( LOCTEXT("GenericDetailsTitle", "Details") )
		.TabColorScale( GetTabColorScale() )
		[
			DetailsView.ToSharedRef()
		];
}

FString FSimpleAssetEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Generic Asset ").ToString();
}

TSharedRef<FSimpleAssetEditor> FSimpleAssetEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit )
{
	TSharedRef< FSimpleAssetEditor > NewEditor( new FSimpleAssetEditor() );

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add( ObjectToEdit );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit );

	return NewEditor;
}

TSharedRef<FSimpleAssetEditor> FSimpleAssetEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit )
{
	TSharedRef< FSimpleAssetEditor > NewEditor( new FSimpleAssetEditor() );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit );
	return NewEditor;
}

#undef LOCTEXT_NAMESPACE
