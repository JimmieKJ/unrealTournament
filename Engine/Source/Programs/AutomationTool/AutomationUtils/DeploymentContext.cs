// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

public struct StageTarget
{
	public TargetReceipt Receipt;
	public bool RequireFilesExist;
}

public class DeploymentContext //: ProjectParams
{
	/// <summary>
	/// Full path to the .uproject file
	/// </summary>
	public FileReference RawProjectPath;

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
	/// Receipts for the build targets that should be staged.
	/// </summary>
	public List<StageTarget> StageTargets;

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
	public Dictionary<string, List<string>> NonUFSStagingFiles = new Dictionary<string, List<string>>();
	/// <summary>
	///  After staging, this is a map from source file to relative file in the stage
	/// These file are debug, and can't go into a pak file
	/// </summary>
	public Dictionary<string, List<string>> NonUFSStagingFilesDebug = new Dictionary<string, List<string>>();
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
	///  Directory to project binaries
	/// </summary>
	public string ProjectBinariesFolder;

	/// <summary>
	/// Filename for the manifest of file changes for iterative deployment.
	/// </summary>
	static public readonly string UFSDeployDeltaFileName			= "Manifest_DeltaUFSFiles.txt";	
	static public readonly string NonUFSDeployDeltaFileName			= "Manifest_DeltaNonUFSFiles.txt";

	/// <summary>
	/// Filename for the manifest of files to delete during deployment.
	/// </summary>
	static public readonly string UFSDeployObsoleteFileName = "Manifest_ObsoleteUFSFiles.txt";
	static public readonly string NonUFSDeployObsoleteFileName = "Manifest_ObsoleteNonUFSFiles.txt";

	/// <summary>
	/// The client connects to dedicated server to get data
	/// </summary>
	public bool DedicatedServer;

	/// <summary>
	/// The dedicated server and client use
	/// </summary>
	public bool bUseWebsocketNetDriver;

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

	/// <summary>
	/// If true, multiple platforms are being merged together - some behavior needs to change (but not much)
	/// </summary>
	public bool bIsCombiningMultiplePlatforms = false;

	public DeploymentContext(
		FileReference RawProjectPathOrName,
		string InLocalRoot,
		string BaseStageDirectory,
		string BaseArchiveDirectory,
		Platform InSourcePlatform,
        Platform InTargetPlatform,
		List<UnrealTargetConfiguration> InTargetConfigurations,
		IEnumerable<StageTarget> InStageTargets,
		List<String> InStageExecutables,
		bool InServer,
		bool InCooked,
		bool InStageCrashReporter,
		bool InStage,
		bool InCookOnTheFly,
		bool InArchive,
		bool InProgram,
		bool IsClientInsteadOfNoEditor,
		bool bInUseWebsocketNetDriver = false
		)
	{
		bStageCrashReporter = InStageCrashReporter;
		RawProjectPath = RawProjectPathOrName;
		DedicatedServer = InServer;
		LocalRoot = CommandUtils.CombinePaths(InLocalRoot);
        CookSourcePlatform = InSourcePlatform;
		StageTargetPlatform = InTargetPlatform;
		StageTargetConfigurations = new List<UnrealTargetConfiguration>(InTargetConfigurations);
		StageTargets = new List<StageTarget>(InStageTargets);
		StageExecutables = InStageExecutables;
        IsCodeBasedProject = ProjectUtils.IsCodeBasedUProjectFile(RawProjectPath);
		ShortProjectName = ProjectUtils.GetShortProjectName(RawProjectPath);
		Stage = InStage;
		Archive = InArchive;
		bUseWebsocketNetDriver = bInUseWebsocketNetDriver;

        if (CookSourcePlatform != null && InCooked)
        {
			CookPlatform = CookSourcePlatform.GetCookPlatform(DedicatedServer, IsClientInsteadOfNoEditor);
        }
        else if (CookSourcePlatform != null && InProgram)
        {
            CookPlatform = CookSourcePlatform.GetCookPlatform(false, false);
        }
        else
        {
            CookPlatform = "";
        }

		if (StageTargetPlatform != null && InCooked)
		{
			FinalCookPlatform = StageTargetPlatform.GetCookPlatform(DedicatedServer, IsClientInsteadOfNoEditor);
		}
		else if (StageTargetPlatform != null && InProgram)
		{
            FinalCookPlatform = StageTargetPlatform.GetCookPlatform(false, false);
		}
		else
		{
            FinalCookPlatform = "";
		}

		PlatformDir = StageTargetPlatform.PlatformType.ToString();

        StageDirectory = CommandUtils.CombinePaths(BaseStageDirectory, FinalCookPlatform);
        ArchiveDirectory = CommandUtils.CombinePaths(BaseArchiveDirectory, FinalCookPlatform);

		if (!CommandUtils.FileExists(RawProjectPath.FullName))
		{
			throw new AutomationException("Can't find uproject file {0}.", RawProjectPathOrName);
		}

		ProjectRoot = CommandUtils.CombinePaths(CommandUtils.GetDirectoryName(RawProjectPath.FullName));

		if (!CommandUtils.DirectoryExists(ProjectRoot))
		{
			throw new AutomationException("Project Directory {0} doesn't exist.", ProjectRoot);
		}

        RelativeProjectRootForStage = ShortProjectName;

		ProjectArgForCommandLines = CommandUtils.MakePathSafeToUseWithCommandLine(RawProjectPath.FullName);
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
		ProjectBinariesFolder = CommandUtils.CombinePaths(ProjectUtils.GetClientProjectBinariesRootPath(RawProjectPath, TargetRules.TargetType.Game, IsCodeBasedProject), PlatformDir);
	}

	public void StageFile(StagedFileType FileType, string InputPath, string OutputPath = null, bool bRemap = true)
	{
		InputPath = InputPath.Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);

		if(OutputPath == null)
		{
			if(InputPath.StartsWith(ProjectRoot, StringComparison.InvariantCultureIgnoreCase))
			{
				OutputPath = CommandUtils.CombinePaths(RelativeProjectRootForStage, InputPath.Substring(ProjectRoot.Length).TrimStart('/', '\\'));
			}
			else if (InputPath.StartsWith(LocalRoot, StringComparison.InvariantCultureIgnoreCase))
			{
				OutputPath = CommandUtils.CombinePaths(InputPath.Substring(LocalRoot.Length).TrimStart('/', '\\'));
			}
			else
			{
				throw new AutomationException("Can't deploy {0} because it doesn't start with {1} or {2}", InputPath, ProjectRoot, LocalRoot);
			}
		}

		if(bRemap)
		{
			OutputPath = StageTargetPlatform.Remap(OutputPath);
		}

		if (FileType == StagedFileType.UFS)
		{
			AddUniqueStagingFile(UFSStagingFiles, InputPath, OutputPath);
		}
		else if (FileType == StagedFileType.NonUFS)
		{
			AddStagingFile(NonUFSStagingFiles, InputPath, OutputPath);
		}
		else if (FileType == StagedFileType.DebugNonUFS)
		{
			AddStagingFile(NonUFSStagingFilesDebug, InputPath, OutputPath);
		}
	}

	public void StageBuildProductsFromReceipt(TargetReceipt Receipt, bool RequireDependenciesToExist, bool TreatNonShippingBinariesAsDebugFiles)
	{
		// Stage all the build products needed at runtime
		foreach(BuildProduct BuildProduct in Receipt.BuildProducts)
		{
			// allow missing files if needed
			if (RequireDependenciesToExist == false && File.Exists(BuildProduct.Path) == false)
			{
				continue;
			}

			if(BuildProduct.Type == BuildProductType.Executable || BuildProduct.Type == BuildProductType.DynamicLibrary || BuildProduct.Type == BuildProductType.RequiredResource)
			{
				StagedFileType FileTypeToUse = StagedFileType.NonUFS;
				if (TreatNonShippingBinariesAsDebugFiles && Receipt.Configuration != UnrealTargetConfiguration.Shipping)
				{
					FileTypeToUse = StagedFileType.DebugNonUFS;
				}

				StageFile(FileTypeToUse, BuildProduct.Path);
			}
			else if(BuildProduct.Type == BuildProductType.SymbolFile)
			{
				// Symbol files aren't true dependencies so we can skip if they don't exist
				if (File.Exists(BuildProduct.Path))
				{
					StageFile(StagedFileType.DebugNonUFS, BuildProduct.Path);
				}
			}
		}
	}

	public void StageRuntimeDependenciesFromReceipt(TargetReceipt Receipt, bool RequireDependenciesToExist, bool bUsingPakFile)
	{
		// Patterns to exclude from wildcard searches. Any maps and assets must be cooked. 
		List<string> ExcludePatterns = new List<string>();
		ExcludePatterns.Add(".../*.umap");
		ExcludePatterns.Add(".../*.uasset");

		// Also stage any additional runtime dependencies, like ThirdParty DLLs
		foreach(RuntimeDependency RuntimeDependency in Receipt.RuntimeDependencies)
		{
			foreach(FileReference File in CommandUtils.ResolveFilespec(CommandUtils.RootDirectory, RuntimeDependency.Path, ExcludePatterns))
			{
				// allow missing files if needed
				if ((RequireDependenciesToExist && RuntimeDependency.Type != StagedFileType.DebugNonUFS) || File.Exists())
				{
					bool bRemap = RuntimeDependency.Type != StagedFileType.UFS || !bUsingPakFile;
					StageFile(RuntimeDependency.Type, File.FullName, bRemap: bRemap);
				}
			}
		}
	}

    public void StageFiles(StagedFileType FileType, string InPath, string Wildcard = "*", bool bRecursive = true, string[] ExcludeWildcard = null, string NewPath = null, bool bAllowNone = false, bool bRemap = true, string NewName = null, bool bAllowNotForLicenseesFiles = true, bool bStripFilesForOtherPlatforms = true, bool bConvertToLower = false)
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
                
                if (!bAllowNotForLicenseesFiles && (FileToCopy.Contains("NotForLicensees") || FileToCopy.Contains("NoRedist")))
                {
                    continue;
                }

				if (bStripFilesForOtherPlatforms && !bIsCombiningMultiplePlatforms)
                {
                    bool OtherPlatform = false;
                    foreach (UnrealTargetPlatform Plat in Enum.GetValues(typeof(UnrealTargetPlatform)))
                    {
						bool bMatchesIniPlatform = (AllFile.EndsWith(".ini") && Plat == StageTargetPlatform.IniPlatformType); // filter ini files for the ini file platform
						bool bMatchesTargetPlatform = (Plat == StageTargetPlatform.PlatformType || Plat == UnrealTargetPlatform.Unknown); // filter platform files for the target platform
						if (!bMatchesIniPlatform && !bMatchesTargetPlatform)
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
							if (Search.StartsWith(ProjectRoot, StringComparison.InvariantCultureIgnoreCase))
							{
								if (ProjectRoot.EndsWith("\\") || ProjectRoot.EndsWith("/"))
								{
									Search = Search.Substring(ProjectRoot.Length - 1);
								}
								else
								{
									Search = Search.Substring(ProjectRoot.Length);
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

                if (bConvertToLower)
                {
                    Dest = Dest.ToLowerInvariant();
                }

				if (FileType == StagedFileType.UFS)
				{
					AddUniqueStagingFile(UFSStagingFiles, FileToCopy, Dest);
				}
				else if (FileType == StagedFileType.NonUFS)
				{
					AddStagingFile(NonUFSStagingFiles, FileToCopy, Dest);
				}
				else if (FileType == StagedFileType.DebugNonUFS)
				{
					AddStagingFile(NonUFSStagingFilesDebug, FileToCopy, Dest);
				}
				FilesAdded++;
			}
		}

		if (FilesAdded == 0 && !bAllowNone && !bIsCombiningMultiplePlatforms)
		{
			throw new AutomationException(ExitCode.Error_StageMissingFile, "No files found to deploy for {0} with wildcard {1} and exclusions {2}", InPath, Wildcard, ExcludeWildcard);
		}

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

	private void AddStagingFile(Dictionary<string, List<string>> FilesToStage, string FileToCopy, string Dest)
	{
		List<string> ExistingDest;
		if (FilesToStage.TryGetValue(FileToCopy, out ExistingDest))
		{
			if (!FilesToStage[FileToCopy].Contains(Dest))
			{
				FilesToStage[FileToCopy].Add(Dest);
			}
		}
		else
		{
			FilesToStage.Add(FileToCopy, new List<string>(){Dest});
		}
	}

	public int ArchiveFiles(string InPath, string Wildcard = "*", bool bRecursive = true, string[] ExcludeWildcard = null, string NewPath = null)
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

				if (!bIsCombiningMultiplePlatforms)
				{
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
							if (Search.StartsWith(ProjectRoot, StringComparison.InvariantCultureIgnoreCase))
							{
								if (ProjectRoot.EndsWith("\\") || ProjectRoot.EndsWith("/"))
								{
									Search = Search.Substring(ProjectRoot.Length - 1);
								}
								else
								{
									Search = Search.Substring(ProjectRoot.Length);
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
				}

				string Dest;
				if (!FileToCopy.StartsWith(InPath))
				{
					throw new AutomationException("Can't archive {0}; it was supposed to start with {1}", FileToCopy, InPath);
				}

				// If the specified a new directory, first we deal with that, then apply the other things
				// this is used to collapse the sandbox, among other things
				if (NewPath != null)
				{
					Dest = FileToCopy.Substring(InPath.Length);
					if (Dest.StartsWith("/") || Dest.StartsWith("\\"))
					{
						Dest = Dest.Substring(1);
					}
					Dest = CommandUtils.CombinePaths(NewPath, Dest);
				}
				else
				{
					Dest = FileToCopy.Substring(InPath.Length);
				}

				if (Dest.StartsWith("/") || Dest.StartsWith("\\"))
				{
					Dest = Dest.Substring(1);
				}

				if (ArchivedFiles.ContainsKey(FileToCopy))
				{
					if (ArchivedFiles[FileToCopy] != Dest)
					{
						throw new AutomationException("Can't archive {0}: it was already in the files to archive with a different destination '{1}'", FileToCopy, Dest);
					}
				}
				else
				{
					ArchivedFiles.Add(FileToCopy, Dest);
				}

				FilesAdded++;
			}
		}

		return FilesAdded;
	}

	public String GetUFSDeploymentDeltaPath(string DeviceName)
	{
		return Path.Combine(StageDirectory, UFSDeployDeltaFileName + DeviceName);
	}

	public String GetNonUFSDeploymentDeltaPath(string DeviceName)
	{
		return Path.Combine(StageDirectory, NonUFSDeployDeltaFileName + DeviceName);
	}

	public String GetUFSDeploymentObsoletePath(string DeviceName)
	{
		return Path.Combine(StageDirectory, UFSDeployObsoleteFileName + DeviceName);
	}

	public String GetNonUFSDeploymentObsoletePath(string DeviceName)
	{
		return Path.Combine(StageDirectory, NonUFSDeployObsoleteFileName + DeviceName);
	}

	public string UFSDeployedManifestFileName
	{
		get
		{
			return "Manifest_UFSFiles_" + StageTargetPlatform.PlatformType.ToString() + ".txt";
		}
	}

	public string NonUFSDeployedManifestFileName
	{
		get
		{
			return "Manifest_NonUFSFiles_" + StageTargetPlatform.PlatformType.ToString() + ".txt";
		}
	}

	public static string GetNonUFSDeployedManifestFileName(UnrealTargetPlatform PlatformType)
	{
		return "Manifest_NonUFSFiles_" + PlatformType.ToString() + ".txt";
	}

	public static string GetUFSDeployedManifestFileName(UnrealTargetPlatform PlatformType)
	{
		return "Manifest_UFSFiles_" + PlatformType.ToString() + ".txt";
	}

	public static string GetDebugFilesManifestFileName(UnrealTargetPlatform PlatformType)
	{
		return "Manifest_DebugFiles_" + PlatformType.ToString() + ".txt";
	}
}
