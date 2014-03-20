﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;

namespace AutomationTool
{
	/// <summary>
	/// Host platform abstraction
	/// </summary>
	public abstract class HostPlatform
	{
		private static HostPlatform RunningPlatform;
		/// <summary>
		/// Current running host platform.
		/// </summary>
		public static HostPlatform Current
		{
			get { return RunningPlatform; }
		}

		/// <summary>
		/// Initializes the current platform.
		/// </summary>
		internal static void Initialize()
		{
			if (UnrealBuildTool.Utils.IsRunningOnMono)
			{
				RunningPlatform = new MacHostPlatform();
			}
			else
			{
				RunningPlatform = new WindowsHostPlatform();
			}
		}

		/// <summary>
		/// Sets .Net framework environment variables.
		/// </summary>
		abstract public void SetFrameworkVars();

		/// <summary>
		/// Gets the build executable filename.
		/// </summary>
		/// <returns></returns>
		abstract public string GetMsBuildExe();

		/// <summary>
		/// Gets the dev environment executable name.
		/// </summary>
		/// <returns></returns>
		abstract public string GetMsDevExe();

		/// <summary>
		/// Folder under UE4/ to the platform's binaries.
		/// </summary>
		abstract public string RelativeBinariesFolder { get; }

		/// <summary>
		/// Full path to the UE4 Editor executable for the current platform.
		/// </summary>
		/// <param name="UE4Exe"></param>
		/// <returns></returns>
		abstract public string GetUE4ExePath(string UE4Exe);

		/// <summary>
		/// Log folder for local builds.
		/// </summary>
		abstract public string LocalBuildsLogFolder { get; }

		/// <summary>
		/// Name of the p4 executable.
		/// </summary>
		abstract public string P4Exe { get; }

		/// <summary>
		/// Creates a process and sets it up for the current platform.
		/// </summary>
		/// <param name="LogName"></param>
		/// <returns></returns>
		abstract public Process CreateProcess(string AppName);

		/// <summary>
		/// Sets any additional options for running an executable.
		/// </summary>
		/// <param name="AppName"></param>
		/// <param name="Options"></param>
		/// <param name="CommandLine"></param>
		abstract public void SetupOptionsForRun(ref string AppName, ref CommandUtils.ERunOptions Options, ref string CommandLine);

		/// <summary>
		/// Sets the console control handler for the current platform.
		/// </summary>
		/// <param name="Handler"></param>
		abstract public void SetConsoleCtrlHandler(ProcessManager.CtrlHandlerDelegate Handler);

		/// <summary>
		/// Gets UBT project name for the current platform.
		/// </summary>
		abstract public string UBTProjectName { get; }

		/// <summary>
		/// Returns the type of the host editor platform.
		/// </summary>
		abstract public UnrealBuildTool.UnrealTargetPlatform HostEditorPlatform { get; }

		/// <summary>
		/// Returns the pdb file extenstion for the host platform.
		/// </summary>
		abstract public string PdbExtension { get; }
	}
}
