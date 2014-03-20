// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SoundClassEditorPrivatePCH.h"
#include "SoundClassEditorModule.h"
#include "SoundClassEditor.h"
#include "ModuleManager.h"
#include "SoundDefinitions.h"

const FName SoundClassEditorAppIdentifier = FName(TEXT("SoundClassEditorApp"));



/**
 * Sound class editor module
 */
class FSoundClassEditorModule : public ISoundClassEditorModule
{
public:
	/** Constructor, set up console commands and variables **/
	FSoundClassEditorModule()
	{
	}

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() OVERRIDE
	{
		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	}

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() OVERRIDE
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();
	}

	/**
	 * Creates a new sound class editor for a sound class object
	 */
	virtual TSharedRef<FAssetEditorToolkit> CreateSoundClassEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundClass* InSoundClass ) OVERRIDE
	{
		TSharedRef<FSoundClassEditor> NewSoundClassEditor(new FSoundClassEditor());
		NewSoundClassEditor->InitSoundClassEditor(Mode, InitToolkitHost, InSoundClass);
		return NewSoundClassEditor;
	}

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() {return ToolBarExtensibilityManager;}

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};

IMPLEMENT_MODULE( FSoundClassEditorModule, SoundClassEditor );