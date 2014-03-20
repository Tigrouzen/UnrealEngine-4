// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MinidumpDiagnostics : ModuleRules
{
	public MinidumpDiagnostics( TargetInfo Target )
	{
		PrivateIncludePathModuleNames.Add( "Launch" );
		PrivateIncludePaths.Add( "Runtime/Launch/Private" );
	
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"CrashDebugHelper",
				"PerforceSourceControl",
				"Projects"
			}
			);

		// Need database support!
		Definitions.Add( "WITH_DATABASE_SUPPORT=1" );
	}
}
