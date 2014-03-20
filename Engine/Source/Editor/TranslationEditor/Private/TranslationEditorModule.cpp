// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "TranslationEditorPrivatePCH.h"
#include "ModuleManager.h"
#include "TranslationEditor.h"
//#include "Toolkits/ToolkitManager.h"
#include "WorkspaceMenuStructureModule.h"
#include "TranslationDataObject.h"
#include "MessageLogModule.h"

class FTranslationEditor;

IMPLEMENT_MODULE( FTranslationEditorModule, TranslationEditor );

#define LOCTEXT_NAMESPACE "TranslationEditorModule"

const FName FTranslationEditorModule::TranslationEditorAppIdentifier( TEXT( "TranslationEditorApp" ) );

void FTranslationEditorModule::StartupModule()
{
#if WITH_UNREAL_DEVELOPER_TOOLS
	// create a message log for source control to use
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing("TranslationEditor", LOCTEXT("TranslationEditorLogLabel", "Translation Editor"));
#endif

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolbarExtensibilityManager = MakeShareable(new FExtensibilityManager);
}

void FTranslationEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();

#if WITH_UNREAL_DEVELOPER_TOOLS
	// unregister message log
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.UnregisterLogListing("TranslationEditor");
#endif
}

TSharedRef<FTranslationEditor> FTranslationEditorModule::CreateTranslationEditor( FName ProjectName, FName TranslationTargetLanguage )
{
	TSharedRef< FTranslationDataManager > DataManager = MakeShareable( new FTranslationDataManager(ProjectName, TranslationTargetLanguage) );

	GWarn->BeginSlowTask(LOCTEXT("BuildingUserInterface", "Building Translation Editor UI..."), true);

	TSharedRef< FTranslationEditor > NewTranslationEditor( FTranslationEditor::Create(DataManager, ProjectName, TranslationTargetLanguage) );
	NewTranslationEditor->InitTranslationEditor( EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), DataManager->GetTranslationDataObject() );

	GWarn->EndSlowTask();

	return NewTranslationEditor;
}

#undef LOCTEXT_NAMESPACE