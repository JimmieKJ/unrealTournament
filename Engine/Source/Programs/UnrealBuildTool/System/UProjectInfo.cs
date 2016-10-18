// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Reflection;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	public class UProjectInfo
	{
		public string GameName;
		public string FileName;
		public FileReference FilePath;
		public DirectoryReference Folder;
		public bool bIsCodeProject;

		UProjectInfo(FileReference InFilePath, bool bInIsCodeProject)
		{
			GameName = InFilePath.GetFileNameWithoutExtension();
			FileName = InFilePath.GetFileName();
			FilePath = InFilePath;
			Folder = FilePath.Directory;
			bIsCodeProject = bInIsCodeProject;
		}

		/// <summary>
		/// Map of relative or complete project file names to the project info
		/// </summary>
		static Dictionary<FileReference, UProjectInfo> ProjectInfoDictionary = new Dictionary<FileReference, UProjectInfo>();

		/// <summary>
		/// Map of short project file names to the relative or complete project file name
		/// </summary>
		static Dictionary<string, FileReference> ShortProjectNameDictionary = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Map of target names to the relative or complete project file name
		/// </summary>
		static Dictionary<string, FileReference> TargetToProjectDictionary = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);

		public static bool FindTargetFilesInFolder(DirectoryReference InTargetFolder)
		{
			bool bFoundTargetFiles = false;
			IEnumerable<string> Files;
			if (!Utils.IsRunningOnMono)
			{
				Files = Directory.EnumerateFiles(InTargetFolder.FullName, "*.target.cs", SearchOption.TopDirectoryOnly);
			}
			else
			{
				Files = Directory.GetFiles(InTargetFolder.FullName, "*.Target.cs", SearchOption.TopDirectoryOnly).AsEnumerable();
			}
			foreach (string TargetFilename in Files)
			{
				bFoundTargetFiles = true;
				foreach (KeyValuePair<FileReference, UProjectInfo> Entry in ProjectInfoDictionary)
				{
					FileInfo ProjectFileInfo = new FileInfo(Entry.Key.FullName);
					string ProjectDir = ProjectFileInfo.DirectoryName.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
					if (TargetFilename.StartsWith(ProjectDir, StringComparison.InvariantCultureIgnoreCase))
					{
						FileInfo TargetInfo = new FileInfo(TargetFilename);
						// Strip off the target.cs
						string TargetName = Utils.GetFilenameWithoutAnyExtensions(TargetInfo.Name);
						if (TargetToProjectDictionary.ContainsKey(TargetName) == false)
						{
							TargetToProjectDictionary.Add(TargetName, Entry.Key);
						}
					}
				}
			}
			return bFoundTargetFiles;
		}

		public static bool FindTargetFiles(DirectoryReference CurrentTopDirectory, ref bool bOutFoundTargetFiles)
		{
			// We will only search as deep as the first target file found
			List<DirectoryReference> SubFolderList = new List<DirectoryReference>();

			// Check the root directory
			bOutFoundTargetFiles |= FindTargetFilesInFolder(CurrentTopDirectory);
			if (bOutFoundTargetFiles == false)
			{
				foreach (DirectoryReference TargetFolder in Directory.EnumerateDirectories(CurrentTopDirectory.FullName, "*", SearchOption.TopDirectoryOnly).Select(x => new DirectoryReference(x)))
				{
					SubFolderList.Add(TargetFolder);
					bOutFoundTargetFiles |= FindTargetFilesInFolder(TargetFolder);
				}
			}

			if (bOutFoundTargetFiles == false)
			{
				// Recurse each folders folders
				foreach (DirectoryReference SubFolder in SubFolderList)
				{
					FindTargetFiles(SubFolder, ref bOutFoundTargetFiles);
				}
			}

			return bOutFoundTargetFiles;
		}

		static readonly string RootDirectory = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().GetOriginalLocation()), "..", "..", "..");
		static readonly string EngineSourceDirectory = Path.GetFullPath(Path.Combine(RootDirectory, "Engine", "Source"));

		/// <summary>
		/// Add a single project to the project info dictionary
		/// </summary>
		public static void AddProject(FileReference ProjectFile)
		{
			if (!ProjectInfoDictionary.ContainsKey(ProjectFile))
			{
				DirectoryReference ProjectDirectory = ProjectFile.Directory;

				// Check if it's a code project
				DirectoryReference SourceFolder = DirectoryReference.Combine(ProjectDirectory, "Source");
				DirectoryReference IntermediateSourceFolder = DirectoryReference.Combine(ProjectDirectory, "Intermediate", "Source");
				bool bIsCodeProject = SourceFolder.Exists() || IntermediateSourceFolder.Exists();

				// Create the project, and check the name is unique
				UProjectInfo NewProjectInfo = new UProjectInfo(ProjectFile, bIsCodeProject);
				if (ShortProjectNameDictionary.ContainsKey(NewProjectInfo.GameName))
				{
					UProjectInfo FirstProject = ProjectInfoDictionary[ShortProjectNameDictionary[NewProjectInfo.GameName]];
					throw new BuildException("There are multiple projects with name {0}\n\t* {1}\n\t* {2}\nThis is not currently supported.", NewProjectInfo.GameName, FirstProject.FilePath.FullName, NewProjectInfo.FilePath.FullName);
				}

				// Add it to the name -> project lookups
				ProjectInfoDictionary.Add(ProjectFile, NewProjectInfo);
				ShortProjectNameDictionary.Add(NewProjectInfo.GameName, ProjectFile);

				// Find all Target.cs files if it's a code project
				if (bIsCodeProject)
				{
					bool bFoundTargetFiles = false;
					if (SourceFolder.Exists() && !FindTargetFiles(SourceFolder, ref bFoundTargetFiles))
					{
						Log.TraceVerbose("No target files found under " + SourceFolder);
					}
					if (IntermediateSourceFolder.Exists() && !FindTargetFiles(IntermediateSourceFolder, ref bFoundTargetFiles))
					{
						Log.TraceVerbose("No target files found under " + IntermediateSourceFolder);
					}
				}
			}
		}

		/// <summary>
		/// Discover and fill in the project info
		/// </summary>
		public static void FillProjectInfo()
		{
			DateTime StartTime = DateTime.Now;

			List<DirectoryReference> DirectoriesToSearch = new List<DirectoryReference>();

			// Find all the .uprojectdirs files contained in the root folder and add their entries to the search array
			string EngineSourceDirectory = Path.GetFullPath(Path.Combine(RootDirectory, "Engine", "Source"));

			foreach (FileReference ProjectDirsFile in UnrealBuildTool.RootDirectory.EnumerateFileReferences("*.uprojectdirs", SearchOption.TopDirectoryOnly))
			{
				Log.TraceVerbose("\tFound uprojectdirs file {0}", ProjectDirsFile.FullName);
				foreach(string Line in File.ReadAllLines(ProjectDirsFile.FullName))
				{
					string TrimLine = Line.Trim();
					if(!TrimLine.StartsWith(";"))
					{
						DirectoryReference BaseProjectDir = DirectoryReference.Combine(UnrealBuildTool.RootDirectory, TrimLine);
						if(BaseProjectDir.IsUnderDirectory(UnrealBuildTool.RootDirectory))
						{
							DirectoriesToSearch.Add(BaseProjectDir);
						}
						else
						{
							Log.TraceWarning("Project search path '{0}' is not under root directory, ignoring.", TrimLine);
						}
					}
				}
			}

			Log.TraceVerbose("\tFound {0} directories to search", DirectoriesToSearch.Count);

			foreach (DirectoryReference DirToSearch in DirectoriesToSearch)
			{
				Log.TraceVerbose("\t\tSearching {0}", DirToSearch.FullName);
				if (DirToSearch.Exists())
				{
					foreach (DirectoryReference SubDir in DirToSearch.EnumerateDirectoryReferences("*", SearchOption.TopDirectoryOnly))
					{
						Log.TraceVerbose("\t\t\tFound subdir {0}", SubDir.FullName);
						foreach(FileReference UProjFile in SubDir.EnumerateFileReferences("*.uproject", SearchOption.TopDirectoryOnly))
						{
							Log.TraceVerbose("\t\t\t\t{0}", UProjFile.FullName);
							AddProject(UProjFile);
						}
					}
				}
				else
				{
					Log.TraceVerbose("ProjectInfo: Skipping directory {0} from .uprojectdirs file as it doesn't exist.", DirToSearch);
				}
			}

			DateTime StopTime = DateTime.Now;

			if (BuildConfiguration.bPrintPerformanceInfo)
			{
				TimeSpan TotalProjectInfoTime = StopTime - StartTime;
				Log.TraceInformation("FillProjectInfo took {0} milliseconds", TotalProjectInfoTime.Milliseconds);
			}

			if (UnrealBuildTool.CommandLineContains("-dumpprojectinfo"))
			{
				UProjectInfo.DumpProjectInfo();
			}
		}

		public static void DumpProjectInfo()
		{
			Log.TraceInformation("Dumping project info...");
			Log.TraceInformation("\tProjectInfo");
			foreach (KeyValuePair<FileReference, UProjectInfo> InfoEntry in ProjectInfoDictionary)
			{
				Log.TraceInformation("\t\t" + InfoEntry.Key);
				Log.TraceInformation("\t\t\tName          : " + InfoEntry.Value.FileName);
				Log.TraceInformation("\t\t\tFile Folder   : " + InfoEntry.Value.Folder);
				Log.TraceInformation("\t\t\tCode Project  : " + (InfoEntry.Value.bIsCodeProject ? "YES" : "NO"));
			}
			Log.TraceInformation("\tShortName to Project");
			foreach (KeyValuePair<string, FileReference> ShortEntry in ShortProjectNameDictionary)
			{
				Log.TraceInformation("\t\tShort Name : " + ShortEntry.Key);
				Log.TraceInformation("\t\tProject    : " + ShortEntry.Value);
			}
			Log.TraceInformation("\tTarget to Project");
			foreach (KeyValuePair<string, FileReference> TargetEntry in TargetToProjectDictionary)
			{
				Log.TraceInformation("\t\tTarget     : " + TargetEntry.Key);
				Log.TraceInformation("\t\tProject    : " + TargetEntry.Value);
			}
		}

		/// <summary>
		/// Filter the list of game projects
		/// </summary>
		/// <param name="bOnlyCodeProjects">If true, only return code projects</param>
		/// <param name="GameNameFilter">Game name to filter against. May be null.</param>
		public static List<UProjectInfo> FilterGameProjects(bool bOnlyCodeProjects, string GameNameFilter)
		{
			List<UProjectInfo> Projects = new List<UProjectInfo>();
			foreach (KeyValuePair<FileReference, UProjectInfo> Entry in ProjectInfoDictionary)
			{
				if (!bOnlyCodeProjects || Entry.Value.bIsCodeProject)
				{
					if (string.IsNullOrEmpty(GameNameFilter) || Entry.Value.GameName == GameNameFilter)
					{
						Projects.Add(Entry.Value);
					}
				}
			}
			return Projects;
		}

		/// <summary>
		/// Get the project folder for the given target name
		/// </summary>
		/// <param name="InTargetName">Name of the target of interest</param>
		/// <param name="OutProjectFileName">The project filename</param>
		/// <returns>True if the target was found</returns>
		public static bool TryGetProjectForTarget(string InTargetName, out FileReference OutProjectFileName)
		{
			return TargetToProjectDictionary.TryGetValue(InTargetName, out OutProjectFileName);
		}

		/// <summary>
		/// Get the project folder for the given project name
		/// </summary>
		/// <param name="InProjectName">Name of the project of interest</param>
		/// <param name="OutProjectFileName">The project filename</param>
		/// <returns>True if the target was found</returns>
		public static bool TryGetProjectFileName(string InProjectName, out FileReference OutProjectFileName)
		{
			return ShortProjectNameDictionary.TryGetValue(InProjectName, out OutProjectFileName);
		}

		/// <summary>
		/// Determine if a plugin is enabled for a given project
		/// </summary>
		/// <param name="Project">The project to check</param>
		/// <param name="Plugin">Information about the plugin</param>
		/// <param name="Platform">The target platform</param>
		/// <returns>True if the plugin should be enabled for this project</returns>
		public static bool IsPluginEnabledForProject(PluginInfo Plugin, ProjectDescriptor Project, UnrealTargetPlatform Platform, TargetRules.TargetType Target)
		{
			bool bEnabled = Plugin.Descriptor.bEnabledByDefault;
			if (Project != null && Project.Plugins != null)
			{
				foreach (PluginReferenceDescriptor PluginReference in Project.Plugins)
				{
					if (String.Compare(PluginReference.Name, Plugin.Name, true) == 0)
					{
						bEnabled = PluginReference.IsEnabledForPlatform(Platform) && PluginReference.IsEnabledForTarget(Target);
					}
				}
			}
			return bEnabled;
		}

		/// <summary>
		/// Determine if a plugin is enabled for a given project
		/// </summary>
		/// <param name="Project">The project to check</param>
		/// <param name="Plugin">Information about the plugin</param>
		/// <param name="Platform">The target platform</param>
		/// <returns>True if the plugin should be enabled for this project</returns>
		public static bool IsPluginDescriptorRequiredForProject(PluginInfo Plugin, ProjectDescriptor Project, UnrealTargetPlatform Platform, TargetRules.TargetType TargetType, bool bBuildDeveloperTools, bool bBuildEditor)
		{
			// Check if it's referenced by name from the project descriptor. If it is, we'll need the plugin to be included with the project regardless of whether it has
			// any platform-specific modules or content, just so the runtime can make the call.
			if (Project != null && Project.Plugins != null)
			{
				foreach (PluginReferenceDescriptor PluginReference in Project.Plugins)
				{
					if (String.Compare(PluginReference.Name, Plugin.Name, true) == 0)
					{
						return PluginReference.IsEnabledForPlatform(Platform) && PluginReference.IsEnabledForTarget(TargetType);
					}
				}
			}

			// If the plugin contains content, it should be included for all platforms
			if(Plugin.Descriptor.bCanContainContent)
			{
				return true;
			}

			// Check if the plugin has any modules for the given target
			foreach (ModuleDescriptor Module in Plugin.Descriptor.Modules)
			{
				if(Module.IsCompiledInConfiguration(Platform, TargetType, bBuildDeveloperTools, bBuildEditor))
				{
					return true;
				}
			}

			return false;
		}
	}
}
