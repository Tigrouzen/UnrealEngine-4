// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	class MacPlatform : UEBuildPlatform
	{
        public override bool CanUseXGE()
        {
            return false;
        }

		/**
		 *	Register the platform with the UEBuildPlatform class
		 */
		public override void RegisterBuildPlatform()
		{
			// Register this build platform for Mac
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Mac.ToString());
			UEBuildPlatform.RegisterBuildPlatform(UnrealTargetPlatform.Mac, this);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Mac, UnrealPlatformGroup.Unix);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Mac, UnrealPlatformGroup.Apple);
		}

		/**
		 *	Retrieve the CPPTargetPlatform for the given UnrealTargetPlatform
		 *
		 *	@param	InUnrealTargetPlatform		The UnrealTargetPlatform being build
		 *	
		 *	@return	CPPTargetPlatform			The CPPTargetPlatform to compile for
		 */
		public override CPPTargetPlatform GetCPPTargetPlatform(UnrealTargetPlatform InUnrealTargetPlatform)
		{
			switch (InUnrealTargetPlatform)
			{
				case UnrealTargetPlatform.Mac:
					return CPPTargetPlatform.Mac;
			}
			throw new BuildException("MacPlatform::GetCPPTargetPlatform: Invalid request for {0}", InUnrealTargetPlatform.ToString());
		}

		/**
		 *	Get the extension to use for the given binary type
		 *	
		 *	@param	InBinaryType		The binrary type being built
		 *	
		 *	@return	string				The binary extenstion (ie 'exe' or 'dll')
		 */
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary: 
					return ".dylib";
				case UEBuildBinaryType.Executable:
					return "";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/**
		 *	Get the extension to use for debug info for the given binary type
		 *	
		 *	@param	InBinaryType		The binary type being built
		 *	
		 *	@return	string				The debug info extension (i.e. 'pdb')
		 */
		public override string GetDebugInfoExtension( UEBuildBinaryType InBinaryType )
		{
			return "";
		}

		public override void ModifyNewlyLoadedModule(UEBuildModule InModule, TargetInfo Target)
		{
			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
                bool bBuildShaderFormats = UEBuildConfiguration.bForceBuildShaderFormats;

                if (!UEBuildConfiguration.bBuildRequiresCookedData)
                {
                    if (InModule.ToString() == "TargetPlatform")
                    {
                        bBuildShaderFormats = true;
                    }
                }

				// allow standalone tools to use target platform modules, without needing Engine
				if (UEBuildConfiguration.bForceBuildTargetPlatforms)
				{
					InModule.AddDynamicallyLoadedModule("MacTargetPlatform");
					InModule.AddDynamicallyLoadedModule("MacNoEditorTargetPlatform");
				}

                if (bBuildShaderFormats)
                {
                    InModule.AddDynamicallyLoadedModule("ShaderFormatOpenGL");
                }
			}
		}
		
		/**
		 *	Setup the target environment for building
		 *	
		 *	@param	InBuildTarget		The target being built
		 */
		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_MAC=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_APPLE=1");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_TTS=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_DATABASE_SUPPORT=0");
		}

		/**
		 *	Whether this platform should create debug information or not
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	@param	InConfiguration		The UnrealTargetConfiguration being built
		 *	
		 *	@return	bool				true if debug info should be generated, false if not
		 */
		public override bool ShouldCreateDebugInfo(UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration)
		{
			return true;
		}

		public override void ResetBuildConfiguration(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			UEBuildConfiguration.bCompileSimplygon = false;
			UEBuildConfiguration.bCompileNetworkProfiler = false;
		}

		public override void ValidateBuildConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
		{
//			BuildConfiguration.bUsePCHFiles = false;
			BuildConfiguration.bCheckExternalHeadersForModification = ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac;
			BuildConfiguration.bCheckSystemHeadersForModification = ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac;
			BuildConfiguration.ProcessorCountMultiplier = MacToolChain.GetAdjustedProcessorCountMultiplier();
			BuildConfiguration.bUseSharedPCHs = false;

			// we always deploy - the build machines need to be able to copy the files back, which needs the full bundle
			BuildConfiguration.bDeployAfterCompile = true;
		}

        public override void ValidateUEBuildConfiguration()
        {
            UEBuildConfiguration.bCompileICU = true;
        }

		/**
		 *	Whether the platform requires the extra UnityCPPWriter
		 *	This is used to add an extra file for UBT to get the #include dependencies from
		 *	
		 *	@return	bool	true if it is required, false if not
		 */
		public override bool RequiresExtraUnityCPPWriter()
		{
			return true;
		}

		/**
		 *	Return whether we wish to have this platform's binaries in our builds
		 */
		public override bool IsBuildRequired()
		{
			return false;
		}

		/**
		 *	Return whether we wish to have this platform's binaries in our CIS tests
		 */
		public override bool IsCISRequired()
		{
			return false;
		}
	}
}
