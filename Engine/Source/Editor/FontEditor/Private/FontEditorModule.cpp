// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "FontEditorModule.h"
#include "ModuleManager.h"
#include "Factories.h"
#include "SFontEditorViewport.h"
#include "FontEditor.h"

const FName FontEditorAppIdentifier = FName(TEXT("FontEditorApp"));


/*-----------------------------------------------------------------------------
   FFontEditorModule
-----------------------------------------------------------------------------*/

class FFontEditorModule : public IFontEditorModule
{
public:
	/** Constructor, set up console commands and variables **/
	FFontEditorModule()
	{
	}

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() OVERRIDE
	{
		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	}

	/** Called before the module is unloaded, right before the module object is destroyed */
	virtual void ShutdownModule() OVERRIDE
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();
	}

	/** Creates a new Font editor */
	virtual TSharedRef<IFontEditor> CreateFontEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UFont* Font ) OVERRIDE
	{
		TSharedRef<FFontEditor> NewFontEditor(new FFontEditor());
		NewFontEditor->InitFontEditor(Mode, InitToolkitHost, Font);
		return NewFontEditor;
	}

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() {return ToolBarExtensibilityManager;}

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};

IMPLEMENT_MODULE( FFontEditorModule, FontEditor );
