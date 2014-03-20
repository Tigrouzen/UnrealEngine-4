// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetTools : ModuleRules
{
	public AssetTools(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Developer/AssetTools/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "CurveAssetEditor",
				"Engine",
                "InputCore",
				"Slate",
                "EditorStyle",
				"SourceControl",
				"TextureEditor",
				"UnrealEd",
				"Kismet"
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics",
				"AssetRegistry",
				"ContentBrowser",
                "CurveAssetEditor",
				"DesktopPlatform",
				"EditorWidgets",
				"Kismet",
				"MainFrame",
				"MaterialEditor",
				"Persona",
				"FontEditor",
				"SoundCueEditor",
				"SoundClassEditor",
				"SourceControl"
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"ContentBrowser",
                "CurveAssetEditor",
				"CurveTableEditor",
				"DataTableEditor",
				"DesktopPlatform",
				"DestructibleMeshEditor",
				"EditorWidgets",
				"PropertyEditor",
				"MainFrame",
				"MaterialEditor",
				"Persona",
				"FontEditor",
				"SoundCueEditor",
				"SoundClassEditor"
			}
			);

	}
}
