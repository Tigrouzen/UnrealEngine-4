﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// The view model for the crash show page.
	/// </summary>
	public class CrashViewModel
	{
		/// <summary>An instance of a crash to interrogate.</summary>
		public Crash Crash { get; set; }
		/// <summary>The callstack associated with the crash.</summary>
		public CallStackContainer CallStack { get; set; }
	}
}