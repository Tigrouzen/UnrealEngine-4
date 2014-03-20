﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel.Design;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using EnvDTE;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Linq;


namespace UnrealVS
{
	public class StartupProjectSelector
	{
		const int StartupProjectComboId = 0x1050;
		const int StartupProjectComboListId = 0x1060;

		public class StartupProjectListChangedEventArgs : EventArgs
		{
			public Project[] StartupProjects { get; set; }
		}
		public delegate void StartupProjectListChangedEventHandler(object sender, StartupProjectListChangedEventArgs e);
		public event StartupProjectListChangedEventHandler StartupProjectListChanged;

		public bool? CachedHideNonGameStartupProjects { get; set; }

		private class ProjectReference
		{
			public Project Project { get; set; }

			public string Name
			{
				get { return (Project != null ? Project.Name : "<invalid>" ); }
			}

			public override string ToString()
			{
				return (Project != null ? Project.Name : "<invalid>" );
			}
		}

		public StartupProjectSelector()
		{
			// Create the handlers for our commands
			{
				// StartupProjectCombo
				var StartupProjectComboCommandId = new CommandID( GuidList.UnrealVSCmdSet, StartupProjectComboId );
				StartupProjectComboCommand = new OleMenuCommand( StartupProjectComboHandler, StartupProjectComboCommandId );
				UnrealVSPackage.Instance.MenuCommandService.AddCommand( StartupProjectComboCommand );

				// StartupProjectComboList
				var StartupProjectComboListCommandId = new CommandID( GuidList.UnrealVSCmdSet, StartupProjectComboListId );
				var StartupProjectComboListCommand = new OleMenuCommand( StartupProjectComboListHandler, StartupProjectComboListCommandId );
				UnrealVSPackage.Instance.MenuCommandService.AddCommand( StartupProjectComboListCommand );
			} 

			// Register for events that we care about
			UnrealVSPackage.Instance.OnStartupProjectChanged += OnStartupProjectChanged;
			UnrealVSPackage.Instance.OnSolutionOpened += delegate { _bIsSolutionOpened = true; UpdateStartupProjectList(Utils.GetAllProjectsFromDTE()); };
			UnrealVSPackage.Instance.OnSolutionClosing += delegate { _bIsSolutionOpened = false; _CachedStartupProjects.Clear(); };
			UnrealVSPackage.Instance.OnProjectOpened += delegate(Project OpenedProject) { if (_bIsSolutionOpened) UpdateStartupProjectList(OpenedProject); };
			UnrealVSPackage.Instance.OnProjectClosed += RemoveFromStartupProjectList;
			UnrealVSPackage.Instance.OptionsPage.OnOptionsChanged += OnOptionsChanged;

			UpdateStartupProjectList(Utils.GetAllProjectsFromDTE());
			UpdateStartupProjectCombo();
		}

		public Project[] StartupProjectList
		{
			get
			{
				return (from ProjectRef in _CachedStartupProjects select ProjectRef.Project).ToArray();
			}
		}

		private void OnOptionsChanged(object Sender, EventArgs E)
		{
			UpdateCachedOptions();

			UpdateStartupProjectList(Utils.GetAllProjectsFromDTE());
		}

		private void UpdateCachedOptions()
		{
			CachedHideNonGameStartupProjects = UnrealVSPackage.Instance.OptionsPage.HideNonGameStartupProjects;
		}

		/// Rebuild/update the list CachedStartupProjectNames whenever something changes
		private void UpdateStartupProjectList(params Project[] DirtyProjects)
		{
			foreach (Project Project in DirtyProjects)
			{
				if (ProjectFilter(Project))
				{
					if (!_CachedStartupProjects.Any(ProjRef => ProjRef.Project == Project))
					{
						_CachedStartupProjects.Add(new ProjectReference { Project = Project });
					}
				}
				else
				{
					RemoveFromStartupProjectList(Project);
				}
			}

			// Sort projects alphabetically
			_CachedStartupProjects.Sort((A, B) => string.CompareOrdinal(A.Name, B.Name));

			if (StartupProjectListChanged != null)
			{
				StartupProjectListChanged(this, new StartupProjectListChangedEventArgs {StartupProjects = StartupProjectList});
			}
		}

		private bool ProjectFilter(Project Project)
		{
			// Initialize the cached options if they haven't been read from UnrealVSOptions yet
			if (!CachedHideNonGameStartupProjects.HasValue)
			{
				UpdateCachedOptions();
			}

			// Optionally, ignore non-game projects
			if (CachedHideNonGameStartupProjects.Value)
			{
				if (!Project.Name.EndsWith("Game", StringComparison.OrdinalIgnoreCase))
				{
					return false;
				}
			}

			// Always filter out non-executable projects
			return Utils.IsProjectExecutable(Project);
		}

		/// Remove projects from the list CachedStartupProjectNames whenever one unloads
		private void RemoveFromStartupProjectList(Project Project)
		{
			_CachedStartupProjects.RemoveAll(ProjRef => ProjRef.Project == Project);

			if (StartupProjectListChanged != null)
			{
				StartupProjectListChanged(this, new StartupProjectListChangedEventArgs { StartupProjects = StartupProjectList });
			}
		}	

		/// <summary>
		/// Updates the combo box after the project loaded state has changed
		/// </summary>
		private void UpdateStartupProjectCombo()
		{
			// Switch to this project!
			IVsHierarchy ProjectHierarchy;
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject( out ProjectHierarchy );
			if( ProjectHierarchy != null )
			{
				StartupProjectComboCommand.Enabled = true;
			}
			else
			{
				StartupProjectComboCommand.Enabled = false;
			}
		}


		/// <summary>
		/// Called when the startup project has changed to a new project.  We'll refresh our interface.
		/// </summary>
		public void OnStartupProjectChanged( Project NewStartupProject )
		{
			UpdateStartupProjectCombo();
		}

	
		/// <summary>
		/// Returns a string to display in the startup project combo box, based on project state
		/// </summary>
		/// <returns>String to display</returns>
		private string MakeStartupProjectComboText()
		{
			string Text = "";

			// Switch to this project!
			IVsHierarchy ProjectHierarchy;
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject( out ProjectHierarchy );
			if( ProjectHierarchy != null )
			{
				var Project = Utils.HierarchyObjectToProject(ProjectHierarchy);
				if (Project != null)
				{
					Text = Project.Name;
				}
			}

			return Text;
		}


		/// Called by combo control to query the text to display or to apply newly-entered text
		void StartupProjectComboHandler( object Sender, EventArgs Args )
		{
			var OleArgs = (OleMenuCmdEventArgs)Args;
			
			// If OutValue is non-zero, then Visual Studio is querying the current combo value to display
			if( OleArgs.OutValue != IntPtr.Zero )
			{
				string ValueString = MakeStartupProjectComboText();
				Marshal.GetNativeVariantForObject( ValueString, OleArgs.OutValue );
			}

			// Has a new input value been entered?
			else if( OleArgs.InValue != null )
			{
				var InputString = OleArgs.InValue.ToString();

				// Lookup the ProjectReference in CachedStartupProjects to get the full name
				ProjectReference ProjectRefMatch = _CachedStartupProjects.FirstOrDefault(ProjRef => ProjRef.Name == InputString);

				if (ProjectRefMatch != null && ProjectRefMatch.Project != null)
					{
						// Switch to this project!
					var ProjectHierarchy = Utils.ProjectToHierarchyObject(ProjectRefMatch.Project);
						UnrealVSPackage.Instance.SolutionBuildManager.set_StartupProject(ProjectHierarchy);
					}					
				}
			}

		/// Called by combo control to populate the drop-down list
		void StartupProjectComboListHandler( object Sender, EventArgs Args )
		{
			var OleArgs = (OleMenuCmdEventArgs)Args;

			var CachedStartupProjectNames = (from ProjRef in _CachedStartupProjects select ProjRef.Name).ToArray();
			Marshal.GetNativeVariantForObject(CachedStartupProjectNames, OleArgs.OutValue);
		}

		/// Combo control for our startup project selector
		OleMenuCommand StartupProjectComboCommand;

		/// Keep track of whether a solution is opened
		bool _bIsSolutionOpened;

		/// Store the list of projects that should be shown in the dropdown list
		readonly List<ProjectReference> _CachedStartupProjects = new List<ProjectReference>();
	}
}
