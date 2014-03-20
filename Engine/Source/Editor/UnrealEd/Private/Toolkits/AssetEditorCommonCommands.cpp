// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AssetEditorCommonCommands.h"



void FAssetEditorCommonCommands::RegisterCommands()
{
	UI_COMMAND( SaveAsset, "Save", "Saves this asset", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( Reimport, "Reimport", "Reimports the asset being edited", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND( SwitchToStandaloneEditor, "Switch to Standalone Editor", "Closes the level-centric asset editor and reopens it in 'standalone' mode", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( SwitchToWorldCentricEditor, "Switch to World-Centric Editor", "Closes the standalone asset editor and reopens it in 'world-centric' mode, docked within the level editor that it was originally opened in.", EUserInterfaceActionType::Button, FInputGesture() );
}
