// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "StaticMeshEditorModule.h"
#include "StaticMeshEditor.h"
#include "ModuleManager.h"

const FName StaticMeshEditorAppIdentifier = FName(TEXT("StaticMeshEditorApp"));

/**
 * StaticMesh editor module
 */
class FStaticMeshEditorModule : public IStaticMeshEditorModule
{
public:
	/** Constructor, set up console commands and variables **/
	FStaticMeshEditorModule()
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
	 * Creates a new StaticMesh editor for a StaticMesh
	 */
	virtual TSharedRef<IStaticMeshEditor> CreateStaticMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UStaticMesh* StaticMesh ) OVERRIDE
	{
		TSharedRef<FStaticMeshEditor> NewStaticMeshEditor(new FStaticMeshEditor());
		NewStaticMeshEditor->InitStaticMeshEditor(Mode, InitToolkitHost,StaticMesh);
		return NewStaticMeshEditor;
	}

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() {return ToolBarExtensibilityManager;}

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};

IMPLEMENT_MODULE( FStaticMeshEditorModule, StaticMeshEditor );
