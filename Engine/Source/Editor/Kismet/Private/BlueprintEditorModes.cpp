// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "ScopedTransaction.h"
#include "BlueprintUtilities.h"
#include "StructureEditorUtils.h"
#include "EdGraphUtilities.h"
#include "Toolkits/IToolkitHost.h"

#include "BlueprintEditorCommands.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorModule.h"
#include "BlueprintEditorModes.h"

// Core kismet tabs
#include "SSCSEditor.h"
#include "SSCSEditorViewport.h"
#include "STimelineEditor.h"
#include "SKismetInspector.h"
#include "SBlueprintPalette.h"
#include "SMyBlueprint.h"
// End of core kismet tabs

// Debugging
#include "Debugging/SKismetDebuggingView.h"
#include "Debugging/KismetDebugCommands.h"
// End of debugging

#include "SBlueprintEditorToolbar.h"
#include "FindInBlueprints.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "BlueprintEditorTabs.h"
#include "BlueprintEditorTabFactories.h"

#define LOCTEXT_NAMESPACE "BlueprintEditor"


const FName FBlueprintEditorApplicationModes::StandardBlueprintEditorMode( TEXT("GraphName") );
const FName FBlueprintEditorApplicationModes::BlueprintDefaultsMode( TEXT("DefaultsName") );
const FName FBlueprintEditorApplicationModes::BlueprintComponentsMode( TEXT("ComponentsName") );
const FName FBlueprintEditorApplicationModes::BlueprintInterfaceMode( TEXT("InterfaceName") );
const FName FBlueprintEditorApplicationModes::BlueprintMacroMode( TEXT("MacroName") );

FBlueprintEditorApplicationMode::FBlueprintEditorApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor, FName InModeName, const bool bRegisterViewport, const bool bRegisterDefaultsTab)
	: FApplicationMode(InModeName)
{
	MyBlueprintEditor = InBlueprintEditor;

	// Create the tab factories
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FDebugInfoSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FPaletteSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FMyBlueprintSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FCompilerResultsSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));
	if( bRegisterViewport )
	{
		BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FSCSViewportSummoner(InBlueprintEditor)));
	}
	if( bRegisterDefaultsTab )
	{
		BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FDefaultsEditorSummoner(InBlueprintEditor)));
	}
	if(FStructureEditorUtils::StructureEditingEnabled())
	{
		BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FUserDefinedStructureEditorSummoner(InBlueprintEditor)));
	}

	CoreTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InBlueprintEditor)));

	TabLayout = FTabManager::NewLayout( "Standalone_BlueprintEditor_Layout_v5" )
		->AddArea
		(
			FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient( 0.186721f )
				->SetHideTabWell(true)
				->AddTab( InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.15f)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.70f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.80f )
						->AddTab( "Document", ETabState::ClosedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.20f )
						->AddTab( FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab )
						->AddTab( FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.15f)
					->Split
					(
						FTabManager::NewStack()
						->AddTab( FBlueprintEditorTabs::PaletteID, ETabState::OpenedTab )
					)
				)
			)
		);
	
	// setup toolbar
	//@TODO: Keep this in sync with AnimBlueprintMode.cpp
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintEditorModesToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddScriptingToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddDebuggingToolbar(ToolbarExtender);
}

void FBlueprintEditorApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(CoreTabFactories);
	BP->PushTabFactories(BlueprintEditorOnlyTabFactories);
	BP->PushTabFactories(BlueprintEditorTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FBlueprintEditorApplicationMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();

	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->SaveEditedObjectState();
	BP->GetMyBlueprintWidget()->ClearGraphActionMenuSelection();
}

void FBlueprintEditorApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->RestoreEditedObjectState();
	BP->SetupViewForBlueprintEditingMode();

	FApplicationMode::PostActivateMode();
}


FBlueprintDefaultsApplicationMode::FBlueprintDefaultsApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
	: FApplicationMode(FBlueprintEditorApplicationModes::BlueprintDefaultsMode)
{
	MyBlueprintEditor = InBlueprintEditor;
	
	BlueprintDefaultsTabFactories.RegisterFactory(MakeShareable(new FDefaultsEditorSummoner(InBlueprintEditor)));
	BlueprintDefaultsTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));
	
	TabLayout = FTabManager::NewLayout( "Standalone_BlueprintDefaults_Layout_v4" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient( 0.2f )
				->SetHideTabWell(true)
				->AddTab( InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewStack()
				->SetHideTabWell(true)
				->AddTab( FBlueprintEditorTabs::DefaultEditorID, ETabState::OpenedTab )
			)
		);

	// setup toolbar
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintEditorModesToolbar(ToolbarExtender);
}

void FBlueprintDefaultsApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(BlueprintDefaultsTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FBlueprintDefaultsApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->StartEditingDefaults();

	FApplicationMode::PostActivateMode();
}


FBlueprintComponentsApplicationMode::FBlueprintComponentsApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
	: FApplicationMode(FBlueprintEditorApplicationModes::BlueprintComponentsMode)
{
	MyBlueprintEditor = InBlueprintEditor;
	
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FConstructionScriptEditorSummoner(InBlueprintEditor)));
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FSCSViewportSummoner(InBlueprintEditor)));
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InBlueprintEditor)));
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FDefaultsEditorSummoner(InBlueprintEditor)));
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));
	
	TabLayout = FTabManager::NewLayout( "Standalone_BlueprintComponents_Layout_v5" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient( 0.2f )
				->SetHideTabWell(true)
				->AddTab( InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient( 0.15f )
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.35f )
						->AddTab( FBlueprintEditorTabs::ConstructionScriptEditorID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.65f )
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.85f )
					->SetHideTabWell(true)
					->AddTab( FBlueprintEditorTabs::SCSViewportID, ETabState::OpenedTab )
				)
			)
		);

	// setup toolbar
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintEditorModesToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddComponentsToolbar(ToolbarExtender);
}

void FBlueprintComponentsApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(BlueprintComponentsTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FBlueprintComponentsApplicationMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();

	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->GetSCSEditor()->SetEnabled(true);
	BP->GetSCSEditor()->ClearSelection();
	BP->GetSCSEditor()->UpdateTree();
	BP->GetInspector()->SetEnabled(true);
	BP->GetInspector()->EnableComponentDetailsCustomization(false);
}

void FBlueprintComponentsApplicationMode::PostActivateMode()
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->GetSCSEditor()->ClearSelection();
	BP->GetSCSEditor()->UpdateTree();
	BP->UpdateSCSPreview();
	BP->GetInspector()->EnableComponentDetailsCustomization(true);

	if (BP->GetSCSViewport()->GetIsSimulateEnabled())
	{
		BP->GetSCSEditor()->SetEnabled(false);
		BP->GetInspector()->SetEnabled(false);
	}

	FApplicationMode::PostActivateMode();
}

////////////////////////////////////////
//
FBlueprintInterfaceApplicationMode::FBlueprintInterfaceApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
	: FApplicationMode(FBlueprintEditorApplicationModes::BlueprintInterfaceMode)
{
	MyBlueprintEditor = InBlueprintEditor;
	
	// Create the tab factories
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FDebugInfoSummoner(InBlueprintEditor)));
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FMyBlueprintSummoner(InBlueprintEditor)));
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FCompilerResultsSummoner(InBlueprintEditor)));
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InBlueprintEditor)));

	TabLayout = FTabManager::NewLayout( "Standalone_BlueprintInterface_Layout_v1" )
		->AddArea
		(
			FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient( 0.186721f )
				->SetHideTabWell(true)
				->AddTab( InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.15f)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.70f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.80f )
						->AddTab( "Document", ETabState::ClosedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.20f )
						->AddTab( FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab )
						->AddTab( FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab )
					)
				)
			)
		);

	// setup toolbar
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
}

void FBlueprintInterfaceApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(BlueprintInterfaceTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

////////////////////////////////////////
//
FBlueprintMacroApplicationMode::FBlueprintMacroApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
	: FApplicationMode(FBlueprintEditorApplicationModes::BlueprintMacroMode)
{
	MyBlueprintEditor = InBlueprintEditor;
	
	// Create the tab factories
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FDebugInfoSummoner(InBlueprintEditor)));
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FMyBlueprintSummoner(InBlueprintEditor)));
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FPaletteSummoner(InBlueprintEditor)));
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InBlueprintEditor)));

	TabLayout = FTabManager::NewLayout( "Standalone_BlueprintMacro_Layout_v1" )
		->AddArea
		(
			FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient( 0.2f )
				->SetHideTabWell(true)
				->AddTab( InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.15f)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.70f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.80f )
						->AddTab( "Document", ETabState::ClosedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.20f )
						->AddTab( FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.15f)
					->Split
					(
						FTabManager::NewStack()
						->AddTab( FBlueprintEditorTabs::PaletteID, ETabState::OpenedTab )
					)
				)
			)
		);

	// setup toolbar
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddScriptingToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddDebuggingToolbar(ToolbarExtender);
}

void FBlueprintMacroApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(BlueprintMacroTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

#undef LOCTEXT_NAMESPACE
