﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell.Interop;
using EnvDTE;
using System.Collections.Generic;
using System.Linq;
using VSLangProj;
using System.IO;

namespace UnrealVS
{
	public class Utils
	{
		public class SafeProjectReference
		{
			public string FullName { get; set; }
			public string Name { get; set; }

			public Project GetProjectSlow()
			{
				Project[] Projects = GetAllProjectsFromDTE();
				return Projects.FirstOrDefault(Proj => string.CompareOrdinal(Proj.FullName, FullName) == 0);
			}
		}

		/// <summary>
		/// Converts a Project to an IVsHierarchy
		/// </summary>
		/// <param name="Project">Project object</param>
		/// <returns>IVsHierarchy for the specified project</returns>
		public static IVsHierarchy ProjectToHierarchyObject( Project Project )
		{
			IVsHierarchy HierarchyObject;
			UnrealVSPackage.Instance.SolutionManager.GetProjectOfUniqueName( Project.FullName, out HierarchyObject );
			return HierarchyObject;
		}


		/// <summary>
		/// Converts an IVsHierarchy object to a Project
		/// </summary>
		/// <param name="HierarchyObject">IVsHierarchy object</param>
		/// <returns>Visual Studio project object</returns>
		public static Project HierarchyObjectToProject( IVsHierarchy HierarchyObject )
		{
			// Get the actual Project object from the IVsHierarchy object that was supplied
			object ProjectObject;
			HierarchyObject.GetProperty(VSConstants.VSITEMID_ROOT, (int) __VSHPROPID.VSHPROPID_ExtObject, out ProjectObject);
			return (Project)ProjectObject;
		}

		/// <summary>
		/// Converts an IVsHierarchy object to a config provider interface
		/// </summary>
		/// <param name="HierarchyObject">IVsHierarchy object</param>
		/// <returns>Visual Studio project object</returns>
		public static IVsCfgProvider2 HierarchyObjectToCfgProvider(IVsHierarchy HierarchyObject)
		{
			// Get the actual Project object from the IVsHierarchy object that was supplied
			object BrowseObject;
			HierarchyObject.GetProperty(VSConstants.VSITEMID_ROOT, (int)__VSHPROPID.VSHPROPID_BrowseObject, out BrowseObject);

			IVsCfgProvider2 CfgProvider = null;
			if (BrowseObject != null)
			{
				CfgProvider = GetCfgProviderFromObject(BrowseObject);
			}

			if (CfgProvider == null)
			{
				CfgProvider = GetCfgProviderFromObject(HierarchyObject);
			}

			return CfgProvider;
		}

		private static IVsCfgProvider2 GetCfgProviderFromObject(object SomeObject)
		{
			IVsCfgProvider2 CfgProvider2 = null;

			var GetCfgProvider = SomeObject as IVsGetCfgProvider;
			if (GetCfgProvider != null)
			{
				IVsCfgProvider CfgProvider;
				GetCfgProvider.GetCfgProvider(out CfgProvider);
				if (CfgProvider != null)
				{
					CfgProvider2 = CfgProvider as IVsCfgProvider2;
				}
			}

			if (CfgProvider2 == null)
			{
				CfgProvider2 = SomeObject as IVsCfgProvider2;
			}

			return CfgProvider2;
		}

		/// <summary>
		/// Locates a specific project config property for the active configuration and returns it (or null if not found.)
		/// </summary>
		/// <param name="Project">Project to search the active configuration for the property</param>
		/// <param name="Configuration">Project configuration to edit, or null to use the "active" configuration</param>
		/// <param name="PropertyName">Name of the property</param>
		/// <returns>Property object or null if not found</returns>
		public static Property GetProjectConfigProperty(Project Project, Configuration Configuration, string PropertyName)
		{
			if (Configuration == null)
			{
				Configuration = Project.ConfigurationManager.ActiveConfiguration;
			}
			if (Configuration != null)
			{
				var Properties = Configuration.Properties;
				foreach (var RawProperty in Properties)
				{
					var Property = (Property)RawProperty;
					if (Property.Name.Equals(PropertyName, StringComparison.InvariantCultureIgnoreCase))
					{
						return Property;
					}
				}
			}

			// Not found
			return null;
		}

		/// <summary>
		/// Locates a specific project property for the active configuration and returns it (or null if not found.)
		/// </summary>
		/// <param name="Project">Project to search for the property</param>
		/// <param name="PropertyName">Name of the property</param>
		/// <returns>Property object or null if not found</returns>
		public static Property GetProjectProperty(Project Project, string PropertyName)
		{
			var Properties = Project.Properties;
			foreach (var RawProperty in Properties)
			{
				var Property = (Property)RawProperty;
				if (Property.Name.Equals(PropertyName, StringComparison.InvariantCultureIgnoreCase))
				{
					return Property;
				}
			}

			// Not found
			return null;
		}

		/// <summary>
		/// Locates a specific project property for the active configuration and attempts to set its value
		/// </summary>
		/// <param name="Property">The property object to set</param>
		/// <param name="PropertyValue">Value to set for this property</param>
		public static void SetPropertyValue(Property Property, object PropertyValue)
		{
			Property.Value = PropertyValue;

			// @todo: Not sure if actually needed for command-line property (saved in .user files, not in project)
			// Mark the project as modified
			// @todo: Throws exception for C++ projects, doesn't mark as saved
			//				Project.IsDirty = true;
			//				Project.Saved = false;
		}

		/// <summary>
		/// Helper class used by the GetUIxxx functions below.
		/// Callers use this to easily traverse UIHierarchies.
		/// </summary>
		public class UITreeItem
		{
			public UITreeItem[] Children { get; set; }
			public string Name { get; set; }
			public object Object { get; set; }
		}

		/// <summary>
		/// Converts a UIHierarchy into an easy to use tree of helper class UITreeItem.
		/// </summary>
		public static UITreeItem GetUIHierarchyTree(UIHierarchy Hierarchy)
		{
			return new UITreeItem
			{
				Name = "Root",
				Object = null,
				Children = (from UIHierarchyItem Child in Hierarchy.UIHierarchyItems select GetUIHierarchyTree(Child)).ToArray()
			};
		}

		/// <summary>
		/// Called by the public GetUIHierarchyTree() function above.
		/// </summary>
		private static UITreeItem GetUIHierarchyTree(UIHierarchyItem HierarchyItem)
		{
			return new UITreeItem
			{
				Name = HierarchyItem.Name,
				Object = HierarchyItem.Object,
				Children = (from UIHierarchyItem Child in HierarchyItem.UIHierarchyItems select GetUIHierarchyTree(Child)).ToArray()
			};
		}

		/// <summary>
		/// Helper function to easily extract a list of objects of type T from a UIHierarchy tree.
		/// </summary>
		/// <typeparam name="T">The type of object to find in the tree. Extracts everything that "Is a" T.</typeparam>
		/// <param name="RootItem">The root of the UIHierarchy to search (converted to UITreeItem via GetUIHierarchyTree())</param>
		/// <returns>An enumerable of objects of type T found beneath the root item.</returns>
		public static IEnumerable<T> GetUITreeItemObjectsByType<T>(UITreeItem RootItem) where T : class
		{
			List<T> Results = new List<T>();

			if (RootItem.Object is T)
			{
				Results.Add((T)RootItem.Object);
			}
			foreach (var Child in RootItem.Children)
			{
				Results.AddRange(GetUITreeItemObjectsByType<T>(Child));
			}

			return Results;
		}

		/// <summary>
		/// Helper to check the properties of a project and determine whether it can be run in VS.
		/// Projects that return true can be run in the debugger by pressing the usual Start Debugging (F5) command.
		/// </summary>
		public static bool IsProjectExecutable(Project Project)
		{
			bool IsExecutable = false;

			if (Project.Kind == GuidList.VCSharpProjectKindGuidString)
			{
				// C# project

				Property StartActionProp = GetProjectConfigProperty(Project, null, "StartAction");
				if (StartActionProp != null)
				{
					prjStartAction StartAction = (prjStartAction)StartActionProp.Value;
					if (StartAction == prjStartAction.prjStartActionProject)
					{
						// Project starts the project's output file when run
						Property OutputTypeProp = GetProjectProperty(Project, "OutputType");
						if (OutputTypeProp != null)
						{
							prjOutputType OutputType = (prjOutputType)OutputTypeProp.Value;
							if (OutputType == prjOutputType.prjOutputTypeWinExe ||
								OutputType == prjOutputType.prjOutputTypeExe)
							{
								IsExecutable = true;
							}
						}
					}
					else if (StartAction == prjStartAction.prjStartActionProgram ||
							 StartAction == prjStartAction.prjStartActionURL)
					{
						// Project starts an external program or a URL when run - assume it has been set deliberately to something executable
						IsExecutable = true;
					}
				}
			}
			else if (Project.Kind == GuidList.VCProjectKindGuidString)
			{
				// C++ project 

				SolutionConfiguration SolutionConfig = UnrealVSPackage.Instance.DTE.Solution.SolutionBuild.ActiveConfiguration;
				SolutionContext ProjectSolutionCtxt = SolutionConfig.SolutionContexts.Item(Project.UniqueName);

                // Get the correct config object from the VCProject
                string ActiveConfigName = string.Format(
                    "{0}|{1}",
					ProjectSolutionCtxt.ConfigurationName,
					ProjectSolutionCtxt.PlatformName);

                // Get the VS version-specific VC project object.
				VCProject VCProject = new VCProject(Project, ActiveConfigName);

				if (VCProject != null)
				{
                    // Sometimes the configurations is null.
                    if (VCProject.Configurations != null)
                    {
                        var VCConfigMatch = VCProject.Configurations.FirstOrDefault(VCConfig => VCConfig.Name == ActiveConfigName);

                        if (VCConfigMatch != null)
                        {
                            if (VCConfigMatch.DebugAttach)
                            {
                                // Project attaches to a running process
                                IsExecutable = true;
                            }
                            else
                            {
                                // Project runs its own process

                                if (VCConfigMatch.DebugFlavor == DebuggerFlavor.Remote)
                                {
                                    // Project debugs remotely
                                    if (VCConfigMatch.DebugRemoteCommand.Length != 0)
                                    {
                                        // An remote program is specified to run
                                        IsExecutable = true;
                                    }
                                }
                                else
                                {
                                    // Local debugger

                                    if (VCConfigMatch.DebugCommand.Length != 0 && VCConfigMatch.DebugCommand != "$(TargetPath)")
                                    {
                                        // An external program is specified to run
                                        IsExecutable = true;
                                    }
                                    else
                                    {
                                        // No command so the project runs the target file

                                        if (VCConfigMatch.ConfigType == ConfigType.Application)
                                        {
                                            IsExecutable = true;
                                        }
                                        else if (VCConfigMatch.ConfigType == ConfigType.Generic)
                                        {
                                            // Makefile

                                            if (VCConfigMatch.NMakeToolOutput.Length != 0)
                                            {
                                                string Ext = Path.GetExtension(VCConfigMatch.NMakeToolOutput);
                                                if (0 == string.Compare(Ext, ".exe", StringComparison.InvariantCultureIgnoreCase))
                                                {
                                                    IsExecutable = true;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
				}
			}
			else
			{
				// @todo: support other project types
			}

			return IsExecutable;
		}

		/// <summary>
		/// Helper to check the properties of a project and determine whether it can be built in VS.
		/// </summary>
		public static bool IsProjectBuildable(Project Project)
		{
			return Project.Kind == GuidList.VCSharpProjectKindGuidString || Project.Kind == GuidList.VCProjectKindGuidString;
		}

		/// Helper function to get the full list of all projects in the DTE Solution
		/// Recurses into items because these are actually in a tree structure
		public static Project[] GetAllProjectsFromDTE()
		{
			List<Project> Projects = new List<Project>();

			foreach (Project Project in UnrealVSPackage.Instance.DTE.Solution.Projects)
			{
				Projects.Add(Project);

				if (Project.ProjectItems != null)
				{
					foreach (ProjectItem Item in Project.ProjectItems)
					{
						GetSubProjectsOfProjectItem(Item, Projects);
					}
				}
			}

			return Projects.ToArray();
		}

		/// Called by GetAllProjectsFromDTE() to list items from the project tree
		private static void GetSubProjectsOfProjectItem(ProjectItem Item, List<Project> Projects)
		{
			if (Item.SubProject != null)
			{
				Projects.Add(Item.SubProject);

				if (Item.SubProject.ProjectItems != null)
				{
					foreach (ProjectItem SubItem in Item.SubProject.ProjectItems)
					{
						GetSubProjectsOfProjectItem(SubItem, Projects);
					}
				}
			}
			if (Item.ProjectItems != null)
			{
				foreach (ProjectItem SubItem in Item.ProjectItems)
				{
					GetSubProjectsOfProjectItem(SubItem, Projects);
				}
			}
		}
	}
}
