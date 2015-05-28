// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell.Interop;
using EnvDTE;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using VSLangProj;
using System.IO;
using EnvDTE80;

namespace UnrealVS
{
	public class Utils
	{
		public const string UProjectExtension = "uproject";

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
		public static bool IsProjectSuitable(Project Project)
		{
			try
			{
				Logging.WriteLine("IsProjectExecutable: Attempting to determine if project " + Project.Name + " is executable");

				var ConfigManager = Project.ConfigurationManager;
				if (ConfigManager == null)
				{
					return false;
				}

				var ActiveProjectConfig = Project.ConfigurationManager.ActiveConfiguration;
				if (ActiveProjectConfig != null)
				{
					Logging.WriteLine(
						"IsProjectExecutable: ActiveProjectConfig=\"" + ActiveProjectConfig.ConfigurationName + "|" + ActiveProjectConfig.PlatformName + "\"");
				}
				else
				{
					Logging.WriteLine("IsProjectExecutable: Warning - ActiveProjectConfig is null!");
				}

				bool IsSuitable = false;

				if (Project.Kind.Equals(GuidList.VCSharpProjectKindGuidString, StringComparison.OrdinalIgnoreCase))
				{
					// C# project

					// Chris.Wood
					//Property StartActionProp = GetProjectConfigProperty(Project, null, "StartAction");
					//if (StartActionProp != null)
					//{
					//	prjStartAction StartAction = (prjStartAction)StartActionProp.Value;
					//	if (StartAction == prjStartAction.prjStartActionProject)
					//	{
					//		// Project starts the project's output file when run
					//		Property OutputTypeProp = GetProjectProperty(Project, "OutputType");
					//		if (OutputTypeProp != null)
					//		{
					//			prjOutputType OutputType = (prjOutputType)OutputTypeProp.Value;
					//			if (OutputType == prjOutputType.prjOutputTypeWinExe ||
					//				OutputType == prjOutputType.prjOutputTypeExe)
					//			{
					//				IsSuitable = true;
					//			}
					//		}
					//	}
					//	else if (StartAction == prjStartAction.prjStartActionProgram ||
					//			 StartAction == prjStartAction.prjStartActionURL)
					//	{
					//		// Project starts an external program or a URL when run - assume it has been set deliberately to something executable
					//		IsSuitable = true;
					//	}
					//}

					IsSuitable = true;
				}
				else if (Project.Kind.Equals(GuidList.VCProjectKindGuidString, StringComparison.OrdinalIgnoreCase))
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
									IsSuitable = true;
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
											IsSuitable = true;
										}
									}
									else
									{
										// Local debugger

										if (VCConfigMatch.DebugCommand.Length != 0 && VCConfigMatch.DebugCommand != "$(TargetPath)")
										{
											// An external program is specified to run
											IsSuitable = true;
										}
										else
										{
											// No command so the project runs the target file

											if (VCConfigMatch.ConfigType == ConfigType.Application)
											{
												IsSuitable = true;
											}
											else if (VCConfigMatch.ConfigType == ConfigType.Generic)
											{
												// Makefile

												if (VCConfigMatch.NMakeToolOutput.Length != 0)
												{
													string Ext = Path.GetExtension(VCConfigMatch.NMakeToolOutput);
													if (!IsLibraryFileExtension(Ext))
													{
														IsSuitable = true;
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
					Logging.WriteLine("IsProjectExecutable: Unrecognised 'Kind' in project " + Project.Name + " guid=" + Project.Kind);
				}

				return IsSuitable;
			}
			catch (Exception ex)
			{
				Exception AppEx = new ApplicationException("IsProjectExecutable() failed", ex);
				Logging.WriteLine(AppEx.ToString());
				throw AppEx;
			}
		}

		/// <summary>
		/// Helper to check the file ext of a binary against known library file exts.
		/// FileExt should include the dot e.g. ".dll"
		/// </summary>
		public static bool IsLibraryFileExtension(string FileExt)
		{
			if (FileExt.Equals(".dll", StringComparison.InvariantCultureIgnoreCase)) return true;
			if (FileExt.Equals(".lib", StringComparison.InvariantCultureIgnoreCase)) return true;
			if (FileExt.Equals(".ocx", StringComparison.InvariantCultureIgnoreCase)) return true;
			if (FileExt.Equals(".a", StringComparison.InvariantCultureIgnoreCase)) return true;
			if (FileExt.Equals(".so", StringComparison.InvariantCultureIgnoreCase)) return true;
			if (FileExt.Equals(".dylib", StringComparison.InvariantCultureIgnoreCase)) return true;

			return false;
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
			try
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
			catch (Exception ex)
			{
				Exception AppEx = new ApplicationException("GetAllProjectsFromDTE() failed", ex);
				Logging.WriteLine(AppEx.ToString());
				throw AppEx;
			}
		}

		public static void ExecuteProjectBuild(Project Project,
												string SolutionConfig,
												string SolutionPlatform,
												BatchBuilderToolControl.BuildJob.BuildJobType BuildType,
												Action ExecutingDelegate,
												Action FailedToStartDelegate)
		{
			IVsHierarchy ProjHierarchy = Utils.ProjectToHierarchyObject(Project);

			if (ProjHierarchy != null)
			{
				SolutionConfigurations SolutionConfigs =
					UnrealVSPackage.Instance.DTE.Solution.SolutionBuild.SolutionConfigurations;

				var MatchedSolutionConfig =
					(from SolutionConfiguration2 Sc in SolutionConfigs select Sc).FirstOrDefault(
						Sc =>
						String.CompareOrdinal(Sc.Name, SolutionConfig) == 0 && String.CompareOrdinal(Sc.PlatformName, SolutionPlatform) == 0);

				if (MatchedSolutionConfig != null)
				{
					SolutionContext ProjectSolutionCtxt = MatchedSolutionConfig.SolutionContexts.Item(Project.UniqueName);

					if (ProjectSolutionCtxt != null)
					{
						IVsCfgProvider2 CfgProvider2 = Utils.HierarchyObjectToCfgProvider(ProjHierarchy);
						if (CfgProvider2 != null)
						{
							IVsCfg Cfg;
							CfgProvider2.GetCfgOfName(ProjectSolutionCtxt.ConfigurationName, ProjectSolutionCtxt.PlatformName, out Cfg);

							if (Cfg != null)
							{
								if (ExecutingDelegate != null) ExecutingDelegate();

								int JobResult = VSConstants.E_FAIL;

								if (BuildType == BatchBuilderToolControl.BuildJob.BuildJobType.Build)
								{
									JobResult =
										UnrealVSPackage.Instance.SolutionBuildManager.StartUpdateSpecificProjectConfigurations(
											1,
											new[] { ProjHierarchy },
											new[] { Cfg },
											null,
											new uint[] { 0 },
											null,
											(uint)VSSOLNBUILDUPDATEFLAGS.SBF_OPERATION_BUILD,
											0);
								}
								else if (BuildType == BatchBuilderToolControl.BuildJob.BuildJobType.Rebuild)
								{
									JobResult =
										UnrealVSPackage.Instance.SolutionBuildManager.StartUpdateSpecificProjectConfigurations(
											1,
											new[] { ProjHierarchy },
											new[] { Cfg },
											new uint[] { 0 },
											null,
											null,
											(uint)(VSSOLNBUILDUPDATEFLAGS.SBF_OPERATION_BUILD | VSSOLNBUILDUPDATEFLAGS.SBF_OPERATION_FORCE_UPDATE),
											0);
								}
								else if (BuildType == BatchBuilderToolControl.BuildJob.BuildJobType.Clean)
								{
									JobResult =
										UnrealVSPackage.Instance.SolutionBuildManager.StartUpdateSpecificProjectConfigurations(
											1,
											new[] { ProjHierarchy },
											new[] { Cfg },
											new uint[] { 0 },
											null,
											null,
											(uint)VSSOLNBUILDUPDATEFLAGS.SBF_OPERATION_CLEAN,
											0);
								}

								if (JobResult == VSConstants.S_OK)
								{
									// Job running - show output
									PrepareOutputPane();
								}
								else
								{
									if (FailedToStartDelegate != null) FailedToStartDelegate();
								}
							}
						}
					}
				}
			}
		}

		public static bool IsGameProject(Project Project)
		{
			return GetUProjectNames().Any(UProject => 0 == string.Compare(UProject, Project.Name, StringComparison.OrdinalIgnoreCase));
		}

		/// <summary>
		/// Does the config build something that takes a .uproject on the command line?
		/// </summary>
		public static bool HasUProjectCommandLineArg(string Config)
		{
			return Config.EndsWith("Editor", StringComparison.InvariantCultureIgnoreCase);
		}

		public static string GetUProjectFileName(Project Project)
		{
			return Project.Name + "." + UProjectExtension;
		}

		/// <summary>
		/// Returns all the .uprojects found under the solution root folder.
		/// Returns names only with no path or extension.
		/// </summary>
		public static IEnumerable<string> GetUProjectNames()
		{
			var Folder = GetSolutionFolder();
			if (string.IsNullOrEmpty(Folder))
			{
				return new string[0];
			}

			if (Folder != CachedUProjectRootFolder)
			{
				CachedUProjectRootFolder = Folder;
				var UProjects = Directory.GetFiles(Folder, "*." + UProjectExtension, SearchOption.AllDirectories);
				CachedUProjectNames = (from FullPath in UProjects select Path.GetFileNameWithoutExtension(FullPath)).ToArray();
			}

			return CachedUProjectNames;
		}

		public static void GetSolutionConfigsAndPlatforms(out string[] SolutionConfigs, out string[] SolutionPlatforms)
		{
			var UniqueConfigs = new List<string>();
			var UniquePlatforms = new List<string>();

			SolutionConfigurations DteSolutionConfigs = UnrealVSPackage.Instance.DTE.Solution.SolutionBuild.SolutionConfigurations;
			foreach (SolutionConfiguration2 SolutionConfig in DteSolutionConfigs)
			{
				if (!UniqueConfigs.Contains(SolutionConfig.Name))
				{
					UniqueConfigs.Add(SolutionConfig.Name);
				}
				if (!UniquePlatforms.Contains(SolutionConfig.PlatformName))
				{
					UniquePlatforms.Add(SolutionConfig.PlatformName);
				}
			}

			SolutionConfigs = UniqueConfigs.ToArray();
			SolutionPlatforms = UniquePlatforms.ToArray();
		}

		public static bool SetActiveSolutionConfiguration(string ConfigName, string PlatformName)
		{
			SolutionConfigurations DteSolutionConfigs = UnrealVSPackage.Instance.DTE.Solution.SolutionBuild.SolutionConfigurations;
			foreach (SolutionConfiguration2 SolutionConfig in DteSolutionConfigs)
			{
				if (string.Compare(SolutionConfig.Name, ConfigName, StringComparison.Ordinal) == 0
					&& string.Compare(SolutionConfig.PlatformName, PlatformName, StringComparison.Ordinal) == 0)
				{
					SolutionConfig.Activate();
					return true;
				}
			}
			return false;
		}

		private static void PrepareOutputPane()
		{
			UnrealVSPackage.Instance.DTE.ExecuteCommand("View.Output");

			var Pane = UnrealVSPackage.Instance.GetOutputPane();
			if (Pane != null)
			{
				// Clear and activate the output pane.
				Pane.Clear();

				// @todo: Activating doesn't seem to really bring the pane to front like we would expect it to.
				Pane.Activate();
			}
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

		private static string GetSolutionFolder()
		{
			if (!UnrealVSPackage.Instance.DTE.Solution.IsOpen)
			{
				return string.Empty;
			}

			return Path.GetDirectoryName(UnrealVSPackage.Instance.SolutionFilepath);
		}

		private static string CachedUProjectRootFolder = string.Empty;
		private static IEnumerable<string> CachedUProjectNames = new string[0];
	}
}
