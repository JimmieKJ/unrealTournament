// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

public enum StagedFileType
{
	UFS,
	NonUFS,
	DebugNonUFS
}

public class DeploymentContext //: ProjectParams
{
	/// <summary>
	/// Full path where the project exists (For uprojects this should include the uproj filename, otherwise just project folder)
	/// </summary>
	public string RawProjectPath;

	/// <summary>
	///  true if we should stage crash reporter
	/// </summary>
	public bool bStageCrashReporter;

    /// <summary>
    ///  CookPlatform, where to get the cooked data from and use for sandboxes
    /// </summary>
    public string CookPlatform;

	/// <summary>
    ///  FinalCookPlatform, directory to stage and archive the final result to
	/// </summary>
    public string FinalCookPlatform;

    /// <summary>
    ///  Source platform to get the cooked data from
    /// </summary>
    public Platform CookSourcePlatform;

	/// <summary>
	///  Target platform used for sandboxes and stage directory names
	/// </summary>
	public Platform StageTargetPlatform;

	/// <summary>
	///  Configurations to stage. Used to determine which ThirdParty configurations to copy.
	/// </summary>
	public List<UnrealTargetConfiguration> StageTargetConfigurations;

	/// <summary>
	///  this is the root directory that contains the engine: d:\a\UE4\
	/// </summary>
	public string LocalRoot;

	/// <summary>
	///  this is the directory that contains the "game": d:\a\UE4\ShooterGame
	/// </summary>
	public string ProjectRoot;

	/// <summary>
	///  raw name used for platform subdirectories Win32
	/// </summary>
	public string PlatformDir;

	/// <summary>
	///  this is the directory that contains the "game", staged: d:\stagedir\WindowsNoEditor\ShooterGame
	/// </summary>
	public string StageProjectRoot;

	/// <summary>
	///  Directory to put all of the files in: d:\stagedir\WindowsNoEditor
	/// </summary>
	public string StageDirectory;

	/// <summary>
	///  The relative staged project root, what would be tacked on to StageDirectory
	/// </summary>
    public string RelativeProjectRootForStage;

    /// <summary>
    ///  The relative source project root, this will be the short project name for foreign projects, otherwise it is ProjectRoot minus the LocalRoot prefix
    /// </summary>
    public string SourceRelativeProjectRoot;

    /// <summary>
	///  The relative staged project root, used inside the pak file list (different slash convention than above)
	/// </summary>
	public string RelativeProjectRootForUnrealPak;

	/// <summary>
	///  This is what you use to test the engine which uproject you want. Many cases.
	/// </summary>
	public string ProjectArgForCommandLines;

    /// <summary>
    ///  This is the root that the cook source platform would run from
    /// </summary>
    public string CookSourceRuntimeRootDir;

	/// <summary>
	///  This is the root that we are going to run from. Many cases.
	/// </summary>
	public string RuntimeRootDir;

	/// <summary>
	///  This is the project root that we are going to run from. Many cases.
	/// </summary>
	public string RuntimeProjectRootDir;

	/// <summary>
	///  This is the executable we are going to run and stage. Filled in by the platform abstraction.
	/// </summary>
	public string RuntimeExecutable = "";

	/// <summary>
	///  List of executables we are going to stage
	/// </summary>
	public List<string> StageExecutables;

	/// <summary>
	///  Probably going away, used to construct ProjectArgForCommandLines in the case that we are running staged
	/// </summary>
	public string UProjectCommandLineArgInternalRoot = "../../../";

	/// <summary>
	///  Probably going away, used to construct the pak file list
	/// </summary>
	public string PakFileInternalRoot = "../../../";

	/// <summary>
	///  Probably going away, and currently unused, this would be used to build the list of files for the UFS server
	/// </summary>
	public string UnrealFileServerInternalRoot = "../../../";

	/// <summary>
	///  After staging, this is a map from source file to relative file in the stage
	/// These file are binaries, etc and can't go into a pak file
	/// </summary>
	public Dictionary<string, string> NonUFSStagingFiles = new Dictionary<string, string>();
	/// <summary>
	///  After staging, this is a map from source file to relative file in the stage
	/// These file are debug, and can't go into a pak file
	/// </summary>
	public Dictionary<string, string> NonUFSStagingFilesDebug = new Dictionary<string, string>();
	/// <summary>
	///  After staging, this is a map from source file to relative file in the stage
	/// These file are content, and can go into a pak file
	/// </summary>
	public Dictionary<string, string> UFSStagingFiles = new Dictionary<string, string>();
	/// <summary>
	///  List of files to be archived
	/// </summary>
	public Dictionary<string, string> ArchivedFiles = new Dictionary<string, string>();
	/// <summary>
	///  Directory to archive all of the files in: d:\archivedir\WindowsNoEditor
	/// </summary>
	public string ArchiveDirectory;

	/// <summary>
	/// Filename for the manifest of file changes for iterative deployment.
	/// </summary>
	static public readonly string UFSDeployDeltaFileName			= "Manifest_DeltaUFSFiles.txt";	
	static public readonly string NonUFSDeployDeltaFileName			= "Manifest_DeltaNonUFSFiles.txt";

	/// <summary>
	/// Filename for the manifest of files currently deployed on a device.
	/// </summary>
	static public readonly string UFSDeployedManifestFileName		= "Manifest_UFSFiles.txt";
	static public readonly string NonUFSDeployedManifestFileName	= "Manifest_NonUFSFiles.txt";

	


	/// <summary>
	/// The client connects to dedicated server to get data
	/// </summary>
	public bool DedicatedServer;

	/// <summary>
	/// True if this build is staged
	/// </summary>
	public bool Stage;

	/// <summary>
	/// True if this build is archived
	/// </summary>
	public bool Archive;

	/// <summary>
	/// True if this project has code
	/// </summary>	
	public bool IsCodeBasedProject;

	/// <summary>
	/// Project name (name of the uproject file without extension or directory name where the project is localed)
	/// </summary>
	public string ShortProjectName;


	public DeploymentContext(
		string RawProjectPathOrName,
		string InLocalRoot,
		string BaseStageDirectory,
		string BaseArchiveDirectory,
		string CookFlavor,
		Platform InSourcePlatform,
        Platform InTargetPlatform,
		List<UnrealTargetConfiguration> InTargetConfigurations,
		List<String> InStageExecutables,
		bool InServer,
		bool InCooked,
		bool InStageCrashReporter,
		bool InStage,
		bool InCookOnTheFly,
		bool InArchive,
		bool InProgram,
		bool bHasDedicatedServerAndClient
		)
	{
		bStageCrashReporter = InStageCrashReporter;
		RawProjectPath = RawProjectPathOrName;
		DedicatedServer = InServer;
		LocalRoot = CommandUtils.CombinePaths(InLocalRoot);
        CookSourcePlatform = InSourcePlatform;
		StageTargetPlatform = InTargetPlatform;
		StageTargetConfigurations = new List<UnrealTargetConfiguration>(InTargetConfigurations);
		StageExecutables = InStageExecutables;
        IsCodeBasedProject = ProjectUtils.IsCodeBasedUProjectFile(RawProjectPath);
		ShortProjectName = ProjectUtils.GetShortProjectName(RawProjectPath);
		Stage = InStage;
		Archive = InArchive;

        if (CookSourcePlatform != null && InCooked)
        {
            CookPlatform = CookSourcePlatform.GetCookPlatform(DedicatedServer, bHasDedicatedServerAndClient, CookFlavor);
        }
        else if (CookSourcePlatform != null && InProgram)
        {
            CookPlatform = CookSourcePlatform.GetCookPlatform(false, false, "");
        }
        else
        {
            CookPlatform = "";
        }

		if (StageTargetPlatform != null && InCooked)
		{
            FinalCookPlatform = StageTargetPlatform.GetCookPlatform(DedicatedServer, bHasDedicatedServerAndClient, CookFlavor);
		}
		else if (StageTargetPlatform != null && InProgram)
		{
            FinalCookPlatform = StageTargetPlatform.GetCookPlatform(false, false, "");
		}
		else
		{
            FinalCookPlatform = "";
		}

		PlatformDir = StageTargetPlatform.PlatformType.ToString();

        StageDirectory = CommandUtils.CombinePaths(BaseStageDirectory, FinalCookPlatform);
        ArchiveDirectory = CommandUtils.CombinePaths(BaseArchiveDirectory, FinalCookPlatform);

		if (!CommandUtils.FileExists(RawProjectPath))
		{
			throw new AutomationException("Can't find uproject file {0}.", RawProjectPathOrName);
		}

		ProjectRoot = CommandUtils.CombinePaths(CommandUtils.GetDirectoryName(Path.GetFullPath(RawProjectPath)));

		if (!CommandUtils.DirectoryExists(ProjectRoot))
		{
			throw new AutomationException("Project Directory {0} doesn't exist.", ProjectRoot);
		}

        RelativeProjectRootForStage = ShortProjectName;

		ProjectArgForCommandLines = CommandUtils.MakePathSafeToUseWithCommandLine(RawProjectPath);
		CookSourceRuntimeRootDir = RuntimeRootDir = LocalRoot;
		RuntimeProjectRootDir = ProjectRoot;

        RelativeProjectRootForUnrealPak = CommandUtils.CombinePaths(RelativeProjectRootForStage).Replace("\\", "/");
		if (RelativeProjectRootForUnrealPak.StartsWith("/"))
		{
			RelativeProjectRootForUnrealPak = RelativeProjectRootForUnrealPak.Substring(1);
            RelativeProjectRootForStage = RelativeProjectRootForStage.Substring(1);
		}

        SourceRelativeProjectRoot = RelativeProjectRootForStage; // for foreign projects this doesn't make much sense, but it turns into a noop on staging files
        if (ProjectRoot.StartsWith(LocalRoot, StringComparison.InvariantCultureIgnoreCase))
        {
            SourceRelativeProjectRoot = ProjectRoot.Substring(LocalRoot.Length);
        }
        if (SourceRelativeProjectRoot.StartsWith("/") || SourceRelativeProjectRoot.StartsWith("\\"))
        {
            SourceRelativeProjectRoot = SourceRelativeProjectRoot.Substring(1);
        }        
        
        if (Stage)
		{
			CommandUtils.CreateDirectory(StageDirectory);
            StageProjectRoot = CommandUtils.CombinePaths(StageDirectory, RelativeProjectRootForStage);

			RuntimeRootDir = StageDirectory;
            CookSourceRuntimeRootDir = CommandUtils.CombinePaths(BaseStageDirectory, CookPlatform);
			RuntimeProjectRootDir = StageProjectRoot;
			ProjectArgForCommandLines = CommandUtils.MakePathSafeToUseWithCommandLine(UProjectCommandLineArgInternalRoot + RelativeProjectRootForStage + "/" + ShortProjectName + ".uproject");
		}
		if (Archive)
		{
			CommandUtils.CreateDirectory(ArchiveDirectory);
		}
		ProjectArgForCommandLines = ProjectArgForCommandLines.Replace("\\", "/");
	}

	public int StageFiles(StagedFileType FileType, string InPath, string Wildcard = "*", bool bRecursive = true, string[] ExcludeWildcard = null, string NewPath = null, bool bAllowNone = false, bool bRemap = true, string NewName = null)
	{
		int FilesAdded = 0;
		// make sure any ..'s are removed
		Utils.CollapseRelativeDirectories(ref InPath);

		if (CommandUtils.DirectoryExists(InPath))
		{
			var All = CommandUtils.FindFiles(Wildcard, bRecursive, InPath);

			var Exclude = new HashSet<string>();
			if (ExcludeWildcard != null)
			{
				foreach (var Excl in ExcludeWildcard)
				{
					var Remove = CommandUtils.FindFiles(Excl, bRecursive, InPath);
					foreach (var File in Remove)
					{
						Exclude.Add(CommandUtils.CombinePaths(File));
					}
				}
			}
			foreach (var AllFile in All)
			{
				var FileToCopy = CommandUtils.CombinePaths(AllFile);
				if (Exclude.Contains(FileToCopy))
				{
					continue;
				}
				bool OtherPlatform = false;
				foreach (UnrealTargetPlatform Plat in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (Plat != StageTargetPlatform.PlatformType && Plat != UnrealTargetPlatform.Unknown)
					{
                        var Search = FileToCopy;
                        if (Search.StartsWith(LocalRoot, StringComparison.InvariantCultureIgnoreCase))
                        {
                            if (LocalRoot.EndsWith("\\") || LocalRoot.EndsWith("/"))
                            {
                                Search = Search.Substring(LocalRoot.Length - 1);
                            }
                            else
                            {
                                Search = Search.Substring(LocalRoot.Length);
                            }
                        }
                        if (Search.IndexOf(CommandUtils.CombinePaths("/" + Plat.ToString() + "/"), 0, StringComparison.InvariantCultureIgnoreCase) >= 0)
						{
							OtherPlatform = true;
							break;
						}
					}
				}
				if (OtherPlatform)
				{
					continue;
				}

				string Dest;
				if (!FileToCopy.StartsWith(InPath))
				{
					throw new AutomationException("Can't deploy {0}; it was supposed to start with {1}", FileToCopy, InPath);
				}
				string FileToRemap = FileToCopy;

                // If the specified a new directory, first we deal with that, then apply the other things
                // this is used to collapse the sandbox, among other things
				if (NewPath != null)
				{
					Dest = FileToRemap.Substring(InPath.Length);
					if (Dest.StartsWith("/") || Dest.StartsWith("\\"))
					{
						Dest = Dest.Substring(1);
					}
					Dest = CommandUtils.CombinePaths(NewPath, Dest);
#if false // if the cooker rebases, I don't think we need to ever rebase while staging to a new path
                    if (Dest.StartsWith("/") || Dest.StartsWith("\\"))
                    {
                        Dest = Dest.Substring(1);
                    }
                    // project relative stuff in a collapsed sandbox
                    if (Dest.StartsWith(SourceRelativeProjectRoot, StringComparison.InvariantCultureIgnoreCase))
                    {
                        Dest = Dest.Substring(SourceRelativeProjectRoot.Length);
                        if (Dest.StartsWith("/") || Dest.StartsWith("\\"))
                        {
                            Dest = Dest.Substring(1);
                        }
                        Dest = CommandUtils.CombinePaths(RelativeProjectRootForStage, Dest);
                    }
#endif
                }

                // project relative file
				else if (FileToRemap.StartsWith(ProjectRoot, StringComparison.InvariantCultureIgnoreCase))
				{
					Dest = FileToRemap.Substring(ProjectRoot.Length);
					if (Dest.StartsWith("/") || Dest.StartsWith("\\"))
					{
						Dest = Dest.Substring(1);
					}
                    Dest = CommandUtils.CombinePaths(RelativeProjectRootForStage, Dest);
				}
                // engine relative file
				else if (FileToRemap.StartsWith(LocalRoot, StringComparison.InvariantCultureIgnoreCase))
				{
					Dest = CommandUtils.CombinePaths(FileToRemap.Substring(LocalRoot.Length));
				}
				else
				{
					throw new AutomationException("Can't deploy {0} because it doesn't start with {1} or {2}", FileToRemap, ProjectRoot, LocalRoot);
				}

				if (Dest.StartsWith("/") || Dest.StartsWith("\\"))
				{
					Dest = Dest.Substring(1);
				}

				if (NewName != null)
				{
					Dest = CommandUtils.CombinePaths(Path.GetDirectoryName(Dest), NewName);
				}

				if (bRemap)
				{
					Dest = StageTargetPlatform.Remap(Dest);
				}

				if (FileType == StagedFileType.UFS)
				{
					AddUniqueStagingFile(UFSStagingFiles, FileToCopy, Dest);
				}
				else if (FileType == StagedFileType.NonUFS)
				{
					AddUniqueStagingFile(NonUFSStagingFiles, FileToCopy, Dest);
				}
				else if (FileType == StagedFileType.DebugNonUFS)
				{
					AddUniqueStagingFile(NonUFSStagingFilesDebug, FileToCopy, Dest);
				}
				FilesAdded++;
			}
		}

		if (FilesAdded == 0 && !bAllowNone)
		{
			AutomationTool.ErrorReporter.Error(String.Format("No files found to deploy for {0} with wildcard {1} and exclusions {2}", InPath, Wildcard, ExcludeWildcard), (int)AutomationTool.ErrorCodes.Error_StageMissingFile);
			throw new AutomationException("No files found to deploy for {0} with wildcard {1} and exclusions {2}", InPath, Wildcard, ExcludeWildcard);
		}

		return FilesAdded;
	}

	private void AddUniqueStagingFile(Dictionary<string, string> FilesToStage, string FileToCopy, string Dest)
	{
		string ExistingDest;
		if (FilesToStage.TryGetValue(FileToCopy, out ExistingDest))
		{
			// If the file already exists, it must have the same dest
			if (String.Compare(ExistingDest, Dest, true) != 0)
			{
				throw new AutomationException("Attempting to add \"{0}\" to staging map but it already exists with a different destination (existing: \"{1}\", new: \"{2}\"",
					FileToCopy, ExistingDest, Dest);
			}
		}
		else
		{
			FilesToStage.Add(FileToCopy, Dest);
		}
	}

	public int ArchiveFiles(string InPath, string Wildcard = "*", bool bRecursive = true, string[] ExcludeWildcard = null)
	{
		int FilesAdded = 0;

		if (CommandUtils.DirectoryExists(InPath))
		{
			var All = CommandUtils.FindFiles(Wildcard, bRecursive, InPath);

			var Exclude = new HashSet<string>();
			if (ExcludeWildcard != null)
			{
				foreach (var Excl in ExcludeWildcard)
				{
					var Remove = CommandUtils.FindFiles(Excl, bRecursive, InPath);
					foreach (var File in Remove)
					{
						Exclude.Add(CommandUtils.CombinePaths(File));
					}
				}
			}
			foreach (var AllFile in All)
			{
				var FileToCopy = CommandUtils.CombinePaths(AllFile);
				if (Exclude.Contains(FileToCopy))
				{
					continue;
				}
				bool OtherPlatform = false;
				foreach (UnrealTargetPlatform Plat in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (Plat != StageTargetPlatform.PlatformType && Plat != UnrealTargetPlatform.Unknown && FileToCopy.IndexOf(CommandUtils.CombinePaths("/" + Plat.ToString() + "/"), 0, StringComparison.InvariantCultureIgnoreCase) >= 0)
					{
						OtherPlatform = true;
						break;
					}
				}
				if (OtherPlatform)
				{
					continue;
				}

				string Dest;
				if (!FileToCopy.StartsWith(InPath))
				{
					throw new AutomationException("Can't archive {0}; it was supposed to start with {1}", FileToCopy, InPath);
				}
				Dest = FileToCopy.Substring(InPath.Length);

				if (Dest.StartsWith("/") || Dest.StartsWith("\\"))
				{
					Dest = Dest.Substring(1);
				}

				ArchivedFiles.Add(FileToCopy, Dest);

				FilesAdded++;
			}
		}

		return FilesAdded;
	}

	public String GetUFSDeploymentDeltaPath()
	{
		return Path.Combine(StageDirectory, UFSDeployDeltaFileName);
	}

	public String GetNonUFSDeploymentDeltaPath()
	{
		return Path.Combine(StageDirectory, NonUFSDeployDeltaFileName);
	}
}
