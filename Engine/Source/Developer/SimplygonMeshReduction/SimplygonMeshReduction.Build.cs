// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SimplygonMeshReduction : ModuleRules
{
	public SimplygonMeshReduction(TargetInfo Target)
	{
		PublicIncludePaths.Add("Developer/SimplygonMeshReduction/Public");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("Engine");
		PrivateDependencyModuleNames.Add("RenderCore");
		AddThirdPartyPrivateStaticDependencies(Target, "Simplygon");
		PrivateDependencyModuleNames.Add("RawMesh");
		PrivateIncludePathModuleNames.Add("MeshUtilities");
        PrivateDependencyModuleNames.Add("MeshBoneReduction");
	}
}
