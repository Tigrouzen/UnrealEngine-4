// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Linq;
using System;
using System.Data.Linq;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// Derived information to the default Bugg information from the database.
	/// </summary>
	public partial class Bugg
	{
		/// <summary></summary>
		public EntitySet<Crash> BuggCrashes { get; set; }
		/// <summary></summary>
		public int CrashesInTimeFrame { get; set; }
		/// <summary></summary>
		public string SourceContext { get; set; }

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public EntitySet<Crash> GetCrashes()
		{
			if( BuggCrashes == null )
			{
				BuggCrashes = Crashes;
			}

			int CrashCount = BuggCrashes.Count;
			if( NumberOfCrashes != CrashCount )
			{
				NumberOfCrashes = CrashCount;

				if( NumberOfCrashes > 0 )
				{
					BuggRepository LocalBuggRepository = new BuggRepository();
					LocalBuggRepository.UpdateBuggData( this, BuggCrashes );
				}
			}

			// Just fill the CallStackContainers
			foreach( Crash CurrentCrash in BuggCrashes )
			{
				if( CurrentCrash.CallStackContainer == null )
				{
					CurrentCrash.CallStackContainer = CurrentCrash.GetCallStack();
				}

				if( SourceContext == null )
				{
					SourceContext = CurrentCrash.SourceContext;
				}
			}

			return BuggCrashes;
		}

		/// <summary>
		/// Return the top lines of a callstack.
		/// </summary>
		/// <param name="Max">The number of lines to return.</param>
		/// <returns>A list of callstack entries.</returns>
		public List<string> GetFunctionCalls( int Max )
		{
			BuggRepository LocalBuggRepository = new BuggRepository();
			List<string> Results = LocalBuggRepository.GetFunctionCalls( Pattern );

			return Results;
		}

		/// <summary>
		/// http://www.codeproject.com/KB/linq/linq-to-sql-many-to-many.aspx
		/// </summary>
		private EntitySet<Crash> CrashesCache;

		/// <summary>
		/// 
		/// </summary>
		public EntitySet<Crash> Crashes
		{
			get
			{
				if( CrashesCache == null )
				{
					CrashesCache = new EntitySet<Crash>( OnCrashesAdd, OnCrashesRemove );
					CrashesCache.SetSource( Buggs_Crashes.Select( CrashInstance => CrashInstance.Crash ) );
				}
				return CrashesCache;
			}
			set
			{
				CrashesCache.Assign( value );
			}
		}

		private void OnCrashesAdd( Crash CrashInstance )
		{
			Buggs_Crashes.Add( new Buggs_Crash { Bugg = this, Crash = CrashInstance } );
			SendPropertyChanged( null );
		}

		private void OnCrashesRemove( Crash CrashInstance )
		{
			Buggs_Crash BuggCrash = Buggs_Crashes.FirstOrDefault( BuggCrashInstance => BuggCrashInstance.BuggId == Id && BuggCrashInstance.CrashId == CrashInstance.Id );
			Buggs_Crashes.Remove( BuggCrash );
			SendPropertyChanged( null );
		}
	}

	/// <summary>
	/// Derived information in addition to what is retrieved from the database.
	/// </summary>
	public partial class Crash
	{
		/// <summary>A formatted callstack.</summary>
		public CallStackContainer CallStackContainer { get; set; }

		/// <summary>Resolve the User, supplied by name in older crashes but by ID in newer ones</summary>
		public User User
		{
			get
			{
				return UserById ?? UserByName;
			}
		}

		/// <summary>The user group of the user.</summary>
		public string UserGroupName = "";

		/// <summary>
		/// Return the Url of the log.
		/// </summary>
		/// <returns>The Url of the log file.</returns>
		public string GetLogUrl()
		{
			return Properties.Settings.Default.CrashReporterFiles + Id + "_Launch.log";
		}

		/// <summary>
		/// Return the Url of the minidump.
		/// </summary>
		/// <returns>The Url of the minidump file.</returns>
		public string GetDumpUrl()
		{
			return Properties.Settings.Default.CrashReporterFiles + Id + "_MiniDump.dmp";
		}

		/// <summary>
		/// Return the Url of the diagnostics file.
		/// </summary>
		/// <returns>The Url of the diagnostics file.</returns>
		public string GetDiagnosticsUrl()
		{
			return Properties.Settings.Default.CrashReporterFiles + Id + "_Diagnostics.txt";
		}

		/// <summary>
		/// Return the Url of the Windows Error Report meta data file.
		/// </summary>
		/// <returns>The Url of the meta data file.</returns>
		public string GetMetaDataUrl()
		{
			return Properties.Settings.Default.CrashReporterFiles + Id + "_WERMeta.xml";
		}

		/// <summary>
		/// Return the Url of the report video file.
		/// </summary>
		/// <returns>The Url of the crash report video file.</returns>
		public string GetVideoUrl()
		{
			return Properties.Settings.Default.CrashReporterVideos + Id + "_CrashVideo.avi";
		}

		/// <summary>
		/// Return lines of processed callstack entries.
		/// </summary>
		/// <param name="StartIndex">The starting entry of the callstack.</param>
		/// <param name="Count">The number of lines to return.</param>
		/// <returns>Lines of processed callstack entries.</returns>
		public List<CallStackEntry> GetCallStackEntries( int StartIndex, int Count )
		{
			IEnumerable<CallStackEntry> Results = new List<CallStackEntry>() { new CallStackEntry() };

			if( CallStackContainer != null && CallStackContainer.CallStackEntries != null )
			{
				int MaxCount = Math.Min( Count, CallStackContainer.CallStackEntries.Count );
				Results = CallStackContainer.CallStackEntries.Take( MaxCount );
			}

			return Results.ToList();
		}

		/// <summary>
		/// Return a display friendly version of the time of crash.
		/// </summary>
		/// <returns>A pair of strings represening the date and time of the crash.</returns>
		public string[] GetTimeOfCrash()
		{
			string[] Results = new string[2];
			if( TimeOfCrash.HasValue )
			{
				DateTime LocalTimeOfCrash = TimeOfCrash.Value.ToLocalTime();
				Results[0] = LocalTimeOfCrash.ToShortDateString();
				Results[1] = LocalTimeOfCrash.ToShortTimeString();
			}

			return Results;
		}

		/// <summary>
		/// Gets a formatted callstack for the crash.
		/// </summary>
		/// <returns>A formatted callstack.</returns>
		public CallStackContainer GetCallStack()
		{
			CrashRepository LocalCrashRepository = new CrashRepository();
			return LocalCrashRepository.GetCallStack( this );
		}

		/// <summary>
		/// http://www.codeproject.com/KB/linq/linq-to-sql-many-to-many.aspx
		/// </summary>
		private EntitySet<Bugg> BuggsCache;

		/// <summary>
		/// 
		/// </summary>
		public EntitySet<Bugg> Buggs
		{
			get
			{
				if( BuggsCache == null )
				{
					BuggsCache = new EntitySet<Bugg>( OnBuggsAdd, OnBuggsRemove );
					BuggsCache.SetSource( Buggs_Crashes.Select( c => c.Bugg ) );
				}

				return BuggsCache;
			}
			set
			{
				BuggsCache.Assign( value );
			}
		}

		private void OnBuggsAdd( Bugg BuggInstance )
		{
			Buggs_Crashes.Add( new Buggs_Crash { Crash = this, Bugg = BuggInstance } );
			SendPropertyChanged( null );
		}

		private void OnBuggsRemove( Bugg BuggInstance )
		{
			var BuggCrash = Buggs_Crashes.FirstOrDefault(
				c => c.CrashId == Id
				&& c.BuggId == BuggInstance.Id );
			Buggs_Crashes.Remove( BuggCrash );

			SendPropertyChanged( null );
		}
	}
}
