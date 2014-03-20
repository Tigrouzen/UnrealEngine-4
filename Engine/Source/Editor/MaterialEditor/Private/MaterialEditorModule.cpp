// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "MaterialEditorModule.h"
#include "MaterialEditor.h"
#include "MaterialEditorUtilities.h"
#include "MaterialInstanceEditor.h"
#include "ModuleManager.h"

const FName MaterialEditorAppIdentifier = FName(TEXT("MaterialEditorApp"));
const FName MaterialInstanceEditorAppIdentifier = FName(TEXT("MaterialInstanceEditorApp"));



/**
 * Material editor module
 */
class FMaterialEditorModule : public IMaterialEditorModule
{
public:
	/** Constructor, set up console commands and variables **/
	FMaterialEditorModule()
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
	 * Creates a new material editor, either for a material or a material function
	 */
	virtual TSharedRef<IMaterialEditor> CreateMaterialEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UMaterial* Material ) OVERRIDE
	{
		TSharedRef<FMaterialEditor> NewMaterialEditor(new FMaterialEditor());
		NewMaterialEditor->InitEditorForMaterial(Material);
		NewMaterialEditor->InitMaterialEditor(Mode, InitToolkitHost, Material);
		return NewMaterialEditor;
	}

	virtual TSharedRef<IMaterialEditor> CreateMaterialEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UMaterialFunction* MaterialFunction ) OVERRIDE
	{
		TSharedRef<FMaterialEditor> NewMaterialEditor(new FMaterialEditor());
		NewMaterialEditor->InitEditorForMaterialFunction(MaterialFunction);
		NewMaterialEditor->InitMaterialEditor(Mode, InitToolkitHost, MaterialFunction);
		return NewMaterialEditor;
	}

	virtual TSharedRef<IMaterialEditor> CreateMaterialInstanceEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UMaterialInstance* MaterialInstance ) OVERRIDE
	{
		TSharedRef<FMaterialInstanceEditor> NewMaterialInstanceEditor(new FMaterialInstanceEditor());
		NewMaterialInstanceEditor->InitMaterialInstanceEditor(Mode, InitToolkitHost, MaterialInstance);
		return NewMaterialInstanceEditor;
	}
	
	virtual void GetVisibleMaterialParameters(const class UMaterial* Material, class UMaterialInstance* MaterialInstance, TArray<class FGuid>& VisibleExpressions)
	{
		FMaterialEditorUtilities::GetVisibleMaterialParameters(Material, MaterialInstance, VisibleExpressions);
	}

	/** Gets the extensibility managers for outside entities to extend material editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() {return ToolBarExtensibilityManager;}

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};

IMPLEMENT_MODULE( FMaterialEditorModule, MaterialEditor );
