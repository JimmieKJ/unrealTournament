// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Reflection;
using System.Linq;

namespace UnrealBuildTool
{
	public class UProjectInfo
	{
		public string GameName;
		public string FileName;
		public string FilePath;
		public string Folder;
		public bool bIsCodeProject;

		UProjectInfo(string InFilePath, bool bInIsCodeProject)
		{
			GameName = Path.GetFileNameWithoutExtension(InFilePath);
			FileName = Path.GetFileName(InFilePath);
			FilePath = InFilePath;
			Folder = Path.GetDirectoryName(InFilePath).TrimEnd('\\', '/');
			bIsCodeProject = bInIsCodeProject;
		}

		/** Map of relative or complete project file names to the project info */
		static Dictionary<string, UProjectInfo> ProjectInfoDictionary = new Dictionary<string, UProjectInfo>( StringComparer.InvariantCultureIgnoreCase );
		/** Map of short project file names to the relative or complete project file name */
		static Dictionary<string, string> ShortProjectNameDictionary = new Dictionary<string,string>( StringComparer.InvariantCultureIgnoreCase );
		/** Map of targetnames to the relative or complete project file name */
		static Dictionary<string, string> TargetToProjectDictionary = new Dictionary<string,string>( StringComparer.InvariantCultureIgnoreCase );

		public static bool FindTargetFilesInFolder(string InTargetFolder)
		{
			bool bFoundTargetFiles = false;
			IEnumerable<string> Files;
			if (!Utils.IsRunningOnMono)
			{
				Files = Directory.EnumerateFiles (InTargetFolder, "*.target.cs", SearchOption.TopDirectoryOnly);
			}
			else
			{
				Files = Directory.GetFiles (InTargetFolder, "*.Target.cs", SearchOption.TopDirectoryOnly).AsEnumerable();
			}
			foreach (var TargetFilename in Files)
			{
				bFoundTargetFiles = true;
				foreach (KeyValuePair<string, UProjectInfo> Entry in ProjectInfoDictionary)
				{
					FileInfo ProjectFileInfo = new FileInfo(Entry.Key);
					string ProjectDir = ProjectFileInfo.DirectoryName.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
					if (TargetFilename.StartsWith(ProjectDir, StringComparison.InvariantCultureIgnoreCase))
					{
						FileInfo TargetInfo = new FileInfo(TargetFilename);
						// Strip off the target.cs
						string TargetName = Utils.GetFilenameWithoutAnyExtensions( TargetInfo.Name );
						if (TargetToProjectDictionary.ContainsKey(TargetName) == false)
						{
							TargetToProjectDictionary.Add(TargetName, Entry.Key);
						}
					}
				}
			}
			return bFoundTargetFiles;
		}

		public static bool FindTargetFiles(string InCurrentTopDirectory, ref bool bOutFoundTargetFiles)
		{
			// We will only search as deep as the first target file found
			string CurrentTopDirectory = InCurrentTopDirectory;
			List<string> SubFolderList = new List<string>();

			// Check the root directory
			bOutFoundTargetFiles |= FindTargetFilesInFolder(CurrentTopDirectory);
			if (bOutFoundTargetFiles == false)
			{
				foreach (var TargetFolder in Directory.EnumerateDirectories(CurrentTopDirectory, "*", SearchOption.TopDirectoryOnly))
				{
					SubFolderList.Add(TargetFolder);
					bOutFoundTargetFiles |= FindTargetFilesInFolder(TargetFolder);
				}
			}

			if (bOutFoundTargetFiles == false)
			{
				// Recurse each folders folders
				foreach (var SubFolder in SubFolderList)
				{
					FindTargetFiles(SubFolder, ref bOutFoundTargetFiles);
				}
			}

			return bOutFoundTargetFiles;
		}

		/// <summary>
		/// Add a single project to the project info dictionary
		/// </summary>
		public static void AddProject(string ProjectFile, bool bIsCodeProject)
		{
			UProjectInfo NewProjectInfo = new UProjectInfo(ProjectFile, bIsCodeProject);
			if (!ShortProjectNameDictionary.ContainsKey(NewProjectInfo.GameName))
			{
				ProjectInfoDictionary.Add(ProjectFile, NewProjectInfo);
				ShortProjectNameDictionary.Add(NewProjectInfo.GameName, ProjectFile);
			}
			else
			{
				var FirstProject = ProjectInfoDictionary[ShortProjectNameDictionary[NewProjectInfo.GameName]];
				Log.TraceWarning("There are multiple projects with name {0}\n\t* {1}\n\t* {2}\nThis is not currently supported and only the first one will be maintained by UBT. Please rename.",
					NewProjectInfo.GameName, Path.GetFullPath(FirstProject.FilePath), Path.GetFullPath(NewProjectInfo.FilePath));
			}
		}

		/// <summary>
		/// Discover and fill in the project info
		/// </summary>
		public static void FillProjectInfo()
		{
			DateTime StartTime = DateTime.Now;

			List<string> DirectoriesToSearch = new List<string>();

			// Find all the .uprojectdirs files contained in the root folder and add their entries to the search array
			string RootDirectory = Path.Combine(Utils.GetExecutingAssemblyDirectory(), "..", "..", "..");
			string EngineSourceDirectory = Path.GetFullPath( Path.Combine(RootDirectory, "Engine", "Source") );

			foreach (var File in Directory.EnumerateFiles(RootDirectory, "*.uprojectdirs", SearchOption.TopDirectoryOnly))
			{
				string FilePath = Path.GetFullPath (File);
				Log.TraceVerbose("\tFound uprojectdirs file {0}", FilePath);

				using (StreamReader Reader = new StreamReader(FilePath))
				{
					string LineRead;
					while ((LineRead = Reader.ReadLine()) != null)
					{
						string ProjDirEntry = LineRead.Trim();
						if (String.IsNullOrEmpty(ProjDirEntry) == false)
						{
							if (ProjDirEntry.StartsWith(";"))
							{
								// Commented out line... skip it
								continue;
							}
							else
							{
								string DirPath = Path.GetFullPath (Path.Combine (RootDirectory, ProjDirEntry));
								DirectoriesToSearch.Add(DirPath);
							}
						}
					}
				}
			}

			Log.TraceVerbose("\tFound {0} directories to search", DirectoriesToSearch.Count);

			// Initialize the target finding time to 0
			TimeSpan TotalTargetTime = DateTime.Now - DateTime.Now;

			foreach (string DirToSearch in DirectoriesToSearch)
			{
				Log.TraceVerbose("\t\tSearching {0}", DirToSearch);
				if (Directory.Exists(DirToSearch))
				{
					foreach (string SubDir in Directory.EnumerateDirectories(DirToSearch, "*", SearchOption.TopDirectoryOnly))
					{
						Log.TraceVerbose("\t\t\tFound subdir {0}", SubDir);
						string[] SubDirFiles = Directory.GetFiles(SubDir, "*.uproject", SearchOption.TopDirectoryOnly);
						foreach (string UProjFile in SubDirFiles)
						{
							string RelativePath = Utils.MakePathRelativeTo(UProjFile, EngineSourceDirectory);
							Log.TraceVerbose("\t\t\t\t{0}", RelativePath);
							if( !ProjectInfoDictionary.ContainsKey(RelativePath) )
							{
								DateTime TargetStartTime = DateTime.Now;

								string SourceFolder = Path.Combine(Path.GetDirectoryName(UProjFile), "Source");
								bool bIsCodeProject = Directory.Exists(SourceFolder);

								AddProject(RelativePath, bIsCodeProject);

								if(bIsCodeProject)
								{
									// Find all Target.cs files
									bool bFoundTargetFiles = false;
									if (!FindTargetFiles(SourceFolder, ref bFoundTargetFiles))
									{
										Log.TraceVerbose("No target files found under " + SourceFolder);
									}
								}

								DateTime TargetStopTime = DateTime.Now;

								TotalTargetTime += TargetStopTime - TargetStartTime;
							}
						}
					}
				}
				else
				{
					Log.TraceVerbose("ProjectInfo: Skipping directory {0} from .uprojectdirs file as it doesn't exist.", DirToSearch);
				}
			}

			DateTime StopTime = DateTime.Now;

			if( BuildConfiguration.bPrintPerformanceInfo )
			{
				TimeSpan TotalProjectInfoTime = StopTime - StartTime;
				Log.TraceInformation("FillProjectInfo took {0} milliseconds (AddTargetInfo {1} ms)",
						TotalProjectInfoTime.Milliseconds, TotalTargetTime.Milliseconds);
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
			foreach (KeyValuePair<string, UProjectInfo> InfoEntry in ProjectInfoDictionary)
			{
				Log.TraceInformation("\t\t" + InfoEntry.Key);
				Log.TraceInformation("\t\t\tName          : " + InfoEntry.Value.FileName);
				Log.TraceInformation("\t\t\tFile Folder   : " + InfoEntry.Value.Folder);
				Log.TraceInformation("\t\t\tCode Project  : " + (InfoEntry.Value.bIsCodeProject ? "YES" : "NO"));
			}
			Log.TraceInformation("\tShortName to Project");
			foreach (KeyValuePair<string, string> ShortEntry in ShortProjectNameDictionary)
			{
				Log.TraceInformation("\t\tShort Name : " + ShortEntry.Key);
				Log.TraceInformation("\t\tProject    : " + ShortEntry.Value);
			}
			Log.TraceInformation("\tTarget to Project");
			foreach (KeyValuePair<string, string> TargetEntry in TargetToProjectDictionary)
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
			foreach (KeyValuePair<string, UProjectInfo> Entry in ProjectInfoDictionary)
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
		/// <returns>The project filename, empty string if not found</returns>
		public static string GetProjectForTarget(string InTargetName)
		{
			string ProjectName;
			if (TargetToProjectDictionary.TryGetValue(InTargetName, out ProjectName) == true)
			{
				return ProjectName;
			}
			return "";
		}

		/// <summary>
		/// Get the project folder for the given project name
		/// </summary>
		/// <param name="InProjectName">Name of the project of interest</param>
		/// <returns>The project filename, empty string if not found</returns>
		public static string GetProjectFilePath(string InProjectName)
		{
			string ProjectFilePath;
			if (ShortProjectNameDictionary.TryGetValue(InProjectName, out ProjectFilePath))
			{
				return ProjectFilePath;
			}
			return "";
		}

		/// <summary>
		/// Determine if a plugin is enabled for a given project
		/// </summary>
		/// <param name="Project">The project to check</param>
		/// <param name="Plugin">Information about the plugin</param>
		/// <param name="Platform">The target platform</param>
		/// <returns>True if the plugin should be enabled for this project</returns>
		public static bool IsPluginEnabledForProject(PluginInfo Plugin, ProjectDescriptor Project, UnrealTargetPlatform Platform)
		{
			bool bEnabled = Plugin.Descriptor.bEnabledByDefault || Plugin.LoadedFrom == PluginLoadedFrom.GameProject;
			if(Project != null && Project.Plugins != null)
			{
				foreach(PluginReferenceDescriptor PluginReference in Project.Plugins)
				{
					if(String.Compare(PluginReference.Name, Plugin.Name, true) == 0)
					{
						bEnabled = PluginReference.IsEnabledForPlatform(Platform);
					}
				}
			}
			return bEnabled;
		}
	}
}
