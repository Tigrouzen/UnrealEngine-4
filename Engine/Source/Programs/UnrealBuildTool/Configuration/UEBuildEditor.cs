// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace UnrealBuildTool
{
	public class UEBuildEditor : UEBuildTarget
	{
		public UEBuildEditor(
			string InGameName, 
			UnrealTargetPlatform InPlatform, 
			UnrealTargetConfiguration InConfiguration,
			TargetRules InRulesObject,
			List<string> InAdditionalDefinitions, 
			string InRemoteRoot, 
			List<OnlyModule> InOnlyModules)
			// NOTE: If we're building a monolithic binary, then the game and engine code are linked together into one
			//       program executable, so we want the application name to be the game name.  In the case of a modular
			//       binary, we use 'UnrealEngine' for our application name
			: base(
				InAppName:UEBuildTarget.GetBinaryBaseName(
					InGameName, 
					InRulesObject, 
					InPlatform, 
					InConfiguration, 
					(InRulesObject.Type == TargetRules.TargetType.Editor) ? "Editor" : ""
					),
				InGameName:InGameName, 
				InPlatform:InPlatform, 
				InConfiguration:InConfiguration,
				InRulesObject: InRulesObject, 
				InAdditionalDefinitions:InAdditionalDefinitions, 
				InRemoteRoot:InRemoteRoot, 
				InOnlyModules:InOnlyModules)
		{
		}

		/// <summary>
		/// Setup the binaries for this target
		/// </summary>
		protected override void SetupBinaries()
		{
			base.SetupBinaries();

			{
				// Make the editor executable.
				UEBuildBinaryConfiguration Config = new UEBuildBinaryConfiguration( InType: UEBuildBinaryType.Executable,
																					InOutputFilePath: OutputPath,
																					InIntermediateDirectory: EngineIntermediateDirectory,
																					bInCreateImportLibrarySeparately: (ShouldCompileMonolithic() ? false : true),
																					bInAllowExports:!ShouldCompileMonolithic(),
																					InModuleNames: new List<string>() { "Launch" } );

				AppBinaries.Add( new UEBuildBinaryCPP( this, Config ) );
			}

			// Add the other modules that we want to compile along with the executable.  These aren't necessarily
			// dependencies to any of the other modules we're building, so we need to opt in to compile them.
			{
				// Modules should properly identify the 'extra modules' they need now.
				// There should be nothing here!
			}

			// Allow the platform to setup binaries
			UEBuildPlatform.GetBuildPlatform(Platform).SetupBinaries(this);
		}

		public override void SetupDefaultGlobalEnvironment(
			TargetInfo Target,
			ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
			ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
			)
		{
			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				UEBuildConfiguration.bCompileNetworkProfiler = false;
			}
			else
			{
				UEBuildConfiguration.bCompileNetworkProfiler = true;
			}

			UEBuildConfiguration.bCompileLeanAndMeanUE = false;

			// Do not include the editor
			UEBuildConfiguration.bBuildEditor = true;
			UEBuildConfiguration.bBuildWithEditorOnlyData = true;

			// Require cooked data
			UEBuildConfiguration.bBuildRequiresCookedData = false;

			// Compile the engine
			UEBuildConfiguration.bCompileAgainstEngine = true;

			// Tag it as a 'Editor' build
			OutCPPEnvironmentConfiguration.Definitions.Add("UE_EDITOR=1");
		}
	}
}
