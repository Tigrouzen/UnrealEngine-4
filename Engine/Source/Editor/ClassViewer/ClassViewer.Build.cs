// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClassViewer : ModuleRules
{
	public ClassViewer(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"EditorWidgets",
                "PropertyEditor"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
                "EditorStyle",
				"UnrealEd",
				"PropertyEditor",
				"GameProjectGeneration"
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"EditorWidgets",
				"PropertyEditor"
			}
			);
	}
}
