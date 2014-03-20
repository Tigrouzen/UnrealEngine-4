// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NetworkFileSystem : ModuleRules
	{
		public NetworkFileSystem(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/NetworkFileSystem/Private",
					"Runtime/NetworkFileSystem/Private/Simple",
					"Runtime/NetworkFileSystem/Private/Streaming",
				}
				);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
				"Projects",
				"SandboxFile",
				"TargetPlatform",
			}
		);

		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/NetworkFileSystem/Public",
				"Runtime/NetworkFileSystem/Public/Interfaces",
                "Runtime/CoreUObject/Public/Interfaces",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
				{
					"Core",
					"Sockets",
				}
				);
		}
	}
}
