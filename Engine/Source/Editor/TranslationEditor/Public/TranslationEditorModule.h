// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "Toolkits/IToolkit.h"	// For EAssetEditorMode
#include "Toolkits/AssetEditorToolkit.h" // For FExtensibilityManager
//#include "ITranslationEditor.h"

class UTranslationDataObject;
class FTranslationEditor;

class FTranslationEditorModule : public IModuleInterface,
	public IHasMenuExtensibility
{

public:
	// IModuleInterface
	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;

	/**
	 * Creates an instance of translation editor object.  Only virtual so that it can be called across the DLL boundary.
	 *
	 * @param ProjectName				Name of the project to translate
	 * @param TranslationTargetLanguage	Language to translate to
	 * 
	 * @return	Interface to the new translation editor
	 */
	virtual TSharedRef<FTranslationEditor> CreateTranslationEditor( FName ProjectName, FName TranslationTargetLanguage );

	/** Gets the extensibility managers for outside entities to extend translation editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolbarExtensibilityManager() {return ToolbarExtensibilityManager;}

	/** Translation Editor app identifier string */
	static const FName TranslationEditorAppIdentifier;

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolbarExtensibilityManager;
};


