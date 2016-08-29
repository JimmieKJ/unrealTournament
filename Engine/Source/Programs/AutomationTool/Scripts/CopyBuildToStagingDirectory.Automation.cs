// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Net.NetworkInformation;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading;
using System.Linq;
using UnrealBuildTool;

/// <summary>
/// Helper command used for cooking.
/// </summary>
/// <remarks>
/// Command line parameters used by this command:
/// -clean
/// </remarks>
public partial class Project : CommandUtils
{

	#region Utilities

	private static readonly object SyncLock = new object();

	/// <returns>The path for the BuildPatchTool executable depending on host platform.</returns>
	private static string GetBuildPatchToolExecutable()
	{
		switch (UnrealBuildTool.BuildHostPlatform.Current.Platform)
		{
			case UnrealTargetPlatform.Win32:
				return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Win32/BuildPatchTool.exe");
			case UnrealTargetPlatform.Win64:
				return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Win64/BuildPatchTool.exe");
			case UnrealTargetPlatform.Mac:
				return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Mac/BuildPatchTool");
			case UnrealTargetPlatform.Linux:
				return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Linux/BuildPatchTool");
		}
		throw new AutomationException(string.Format("Unknown host platform for BuildPatchTool - {0}", UnrealBuildTool.BuildHostPlatform.Current.Platform));
	}

	/// <summary>
	/// Checks the existence of the BuildPatchTool executable exists and builds it if it is missing
	/// </summary>
	private static void EnsureBuildPatchToolExists()
	{
		string BuildPatchToolExe = GetBuildPatchToolExecutable();
		if (!CommandUtils.FileExists_NoExceptions(BuildPatchToolExe))
		{
			lock (SyncLock)
			{
				if (!CommandUtils.FileExists_NoExceptions(BuildPatchToolExe))
				{
					UE4BuildUtils.BuildBuildPatchTool(null, UnrealBuildTool.BuildHostPlatform.Current.Platform);
				}
			}
		}
	}

	/// <summary>
	/// Writes a pak response file to disk
	/// </summary>
	/// <param name="Filename"></param>
	/// <param name="ResponseFile"></param>
	private static void WritePakResponseFile(string Filename, Dictionary<string, string> ResponseFile, bool Compressed, bool EncryptIniFiles)
	{
		using (var Writer = new StreamWriter(Filename, false, new System.Text.UTF8Encoding(true)))
		{
			foreach (var Entry in ResponseFile)
			{
				string Line = String.Format("\"{0}\" \"{1}\"", Entry.Key, Entry.Value);
				if (Compressed)
				{
					Line += " -compress";
				}
				
				if (Path.GetExtension(Entry.Key).Contains(".ini") && EncryptIniFiles)
				{
					Line += " -encrypt";
				}
				Writer.WriteLine(Line);
			}
		}
	}

	/// <summary>
	/// Loads streaming install chunk manifest file from disk
	/// </summary>
	/// <param name="Filename"></param>
	/// <returns></returns>
	private static HashSet<string> ReadPakChunkManifest(string Filename)
	{
		var ResponseFile = ReadAllLines(Filename);
		var Result = new HashSet<string>(ResponseFile, StringComparer.InvariantCultureIgnoreCase);
		return Result;
	}

    static public void RunUnrealPak(Dictionary<string, string> UnrealPakResponseFile, string OutputLocation, string EncryptionKeys, string PakOrderFileLocation, string PlatformOptions, bool Compressed, bool EncryptIniFiles, String PatchSourceContentPath)
	{
		if (UnrealPakResponseFile.Count < 1)
		{
			return;
		}
		string PakName = Path.GetFileNameWithoutExtension(OutputLocation);
		string UnrealPakResponseFileName = CombinePaths(CmdEnv.LogFolder, "PakList_" + PakName + ".txt");
		WritePakResponseFile(UnrealPakResponseFileName, UnrealPakResponseFile, Compressed, EncryptIniFiles);

		var UnrealPakExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealPak.exe");

		Log("Running UnrealPak *******");
		string CmdLine = CommandUtils.MakePathSafeToUseWithCommandLine(OutputLocation) + " -create=" + CommandUtils.MakePathSafeToUseWithCommandLine(UnrealPakResponseFileName);
		if (!String.IsNullOrEmpty(EncryptionKeys))
		{
			CmdLine += " -sign=" + CommandUtils.MakePathSafeToUseWithCommandLine(EncryptionKeys);
		}
		if (GlobalCommandLine.Installed)
		{
			CmdLine += " -installed";
		}
		CmdLine += " -order=" + CommandUtils.MakePathSafeToUseWithCommandLine(PakOrderFileLocation);
		if (GlobalCommandLine.UTF8Output)
		{
			CmdLine += " -UTF8Output";
		}
        if (!String.IsNullOrEmpty(PatchSourceContentPath))
        {
            CmdLine += " -generatepatch=" + CommandUtils.MakePathSafeToUseWithCommandLine(PatchSourceContentPath) + " -tempfiles=" + CommandUtils.MakePathSafeToUseWithCommandLine(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "TempFiles"));
        }
		CmdLine += " -multiprocess"; // Prevents warnings about being unable to write to config files
		CmdLine += PlatformOptions;
		RunAndLog(CmdEnv, UnrealPakExe, CmdLine, Options: ERunOptions.Default | ERunOptions.UTF8Output);
		Log("UnrealPak Done *******");
	}

	static public void LogDeploymentContext(DeploymentContext SC)
	{
		LogLog("Deployment Context **************");
		LogLog("ArchiveDirectory = {0}", SC.ArchiveDirectory);
		LogLog("RawProjectPath = {0}", SC.RawProjectPath);
		LogLog("IsCodeBasedUprojectFile = {0}", SC.IsCodeBasedProject);
		LogLog("DedicatedServer = {0}", SC.DedicatedServer);
		LogLog("Stage = {0}", SC.Stage);
		LogLog("StageTargetPlatform = {0}", SC.StageTargetPlatform.PlatformType.ToString());
		LogLog("LocalRoot = {0}", SC.LocalRoot);
		LogLog("ProjectRoot = {0}", SC.ProjectRoot);
		LogLog("PlatformDir = {0}", SC.PlatformDir);
		LogLog("StageProjectRoot = {0}", SC.StageProjectRoot);
		LogLog("ShortProjectName = {0}", SC.ShortProjectName);
		LogLog("StageDirectory = {0}", SC.StageDirectory);
		LogLog("SourceRelativeProjectRoot = {0}", SC.SourceRelativeProjectRoot);
		LogLog("RelativeProjectRootForStage = {0}", SC.RelativeProjectRootForStage);
		LogLog("RelativeProjectRootForUnrealPak = {0}", SC.RelativeProjectRootForUnrealPak);
		LogLog("ProjectArgForCommandLines = {0}", SC.ProjectArgForCommandLines);
		LogLog("RuntimeRootDir = {0}", SC.RuntimeRootDir);
		LogLog("RuntimeProjectRootDir = {0}", SC.RuntimeProjectRootDir);
		LogLog("UProjectCommandLineArgInternalRoot = {0}", SC.UProjectCommandLineArgInternalRoot);
		LogLog("PakFileInternalRoot = {0}", SC.PakFileInternalRoot);
		LogLog("UnrealFileServerInternalRoot = {0}", SC.UnrealFileServerInternalRoot);
		LogLog("End Deployment Context **************");
	}

	public static Dictionary<string, string> ConvertToLower(Dictionary<string, string> Mapping)
	{
		var Result = new Dictionary<string, string>();
		foreach (var Pair in Mapping)
		{
			Result.Add(Pair.Key, Pair.Value.ToLowerInvariant());
		}
		return Result;
	}
	
	public static Dictionary<string, List<string>> ConvertToLower(Dictionary<string, List<string>> Mapping)
	{
		var Result = new Dictionary<string, List<string>>();
		foreach (var Pair in Mapping)
		{
			List<string> NewList = new List<string>();
			foreach(string s in Pair.Value)
			{
				NewList.Add(s.ToLowerInvariant());
			}
			Result.Add(Pair.Key, NewList);
		}
		return Result;
	}

	public static void MaybeConvertToLowerCase(ProjectParams Params, DeploymentContext SC)
	{
		var BuildPlatform = SC.StageTargetPlatform;

		if (BuildPlatform.DeployLowerCaseFilenames(false))
		{
			SC.NonUFSStagingFiles = ConvertToLower(SC.NonUFSStagingFiles);
			SC.NonUFSStagingFilesDebug = ConvertToLower(SC.NonUFSStagingFilesDebug);
		}
		if (Params.UsePak(SC.StageTargetPlatform) && BuildPlatform.DeployPakInternalLowerCaseFilenames())
		{
			SC.UFSStagingFiles = ConvertToLower(SC.UFSStagingFiles);
		}
		else if (!Params.UsePak(SC.StageTargetPlatform) && BuildPlatform.DeployLowerCaseFilenames(true))
		{
			SC.UFSStagingFiles = ConvertToLower(SC.UFSStagingFiles);
		}
	}

    private static void StageLocalizationDataForCulture(DeploymentContext SC, string CultureName, string SourceDirectory, string DestinationDirectory = null, bool bRemap = true)
    {
        CultureName = CultureName.Replace('-', '_');

        string[] LocaleTags = CultureName.Replace('-', '_').Split('_');

        List<string> PotentialParentCultures = new List<string>();
        
        if (LocaleTags.Length > 0)
        {
            if (LocaleTags.Length > 1 && LocaleTags.Length > 2)
            {
                PotentialParentCultures.Add(string.Join("_", LocaleTags[0], LocaleTags[1], LocaleTags[2]));
            }
            if (LocaleTags.Length > 2)
            {
                PotentialParentCultures.Add(string.Join("_", LocaleTags[0], LocaleTags[2]));
            }
            if (LocaleTags.Length > 1)
            {
                PotentialParentCultures.Add(string.Join("_", LocaleTags[0], LocaleTags[1]));
            }
            PotentialParentCultures.Add(LocaleTags[0]);
        }

        string[] FoundDirectories = CommandUtils.FindDirectories(true, "*", false, new string[] { SourceDirectory });
        foreach (string FoundDirectory in FoundDirectories)
        {
            string DirectoryName = CommandUtils.GetLastDirectoryName(FoundDirectory);
            string CanonicalizedPotentialCulture = DirectoryName.Replace('-', '_');

            if (PotentialParentCultures.Contains(CanonicalizedPotentialCulture))
            {
                SC.StageFiles(StagedFileType.UFS, CombinePaths(SourceDirectory, DirectoryName), "*.locres", true, null, DestinationDirectory != null ? CombinePaths(DestinationDirectory, DirectoryName) : null, true, bRemap);
            }
        }
    }

	public static void CreateStagingManifest(ProjectParams Params, DeploymentContext SC)
	{
		if (!Params.Stage)
		{
			return;
		}
		var ThisPlatform = SC.StageTargetPlatform;

		Log("Creating Staging Manifest...");

        if (Params.HasDLCName)
        {
            string DLCName = Params.DLCName;

            // Making a plugin, grab the binaries too
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Plugins", DLCName), "*.uplugin", false, null, null, true);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Plugins", DLCName, "Binaries"), "libUE4-*.so", true, null, null, true);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Plugins", DLCName, "Binaries"), "UE4-*.dll", true, null, null, true);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Plugins", DLCName, "Binaries"), "libUE4Server-*.so", true, null, null, true);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Plugins", DLCName, "Binaries"), "UE4Server-*.dll", true, null, null, true);

            // Put all of the cooked dir into the staged dir
            string PlatformCookDir = CombinePaths(SC.ProjectRoot, "Plugins", DLCName, "Saved", "Cooked", SC.CookPlatform);
            string[] ExcludeWildCards = {"AssetRegistry.bin"};

            // Stage any loose files in the root folder
            SC.StageFiles(StagedFileType.UFS, PlatformCookDir, "*", false, ExcludeWildCards, SC.RelativeProjectRootForStage, true, !Params.UsePak(SC.StageTargetPlatform));

            // Stage each sub directory separately so that we can skip Engine if need be
            string[] SubDirs = CommandUtils.FindDirectories(true, "*", false, new string[] { PlatformCookDir });
            foreach (string SubDir in SubDirs)
            {
                // Dedicated server cook doesn't save shaders so no Engine dir is created
                if ((!SC.DedicatedServer) && (!Params.DLCIncludeEngineContent) && CommandUtils.GetLastDirectoryName(SubDir).Equals("Engine", StringComparison.InvariantCultureIgnoreCase))
                {
                    continue;
                }
                SC.StageFiles(StagedFileType.UFS, SubDir, "*", true, ExcludeWildCards, SC.RelativeProjectRootForStage, true, !Params.UsePak(SC.StageTargetPlatform));
            }

            return;
        }


		ThisPlatform.GetFilesToDeployOrStage(Params, SC);


		// Stage any extra runtime dependencies from the receipts
		foreach(StageTarget Target in SC.StageTargets)
		{
			SC.StageRuntimeDependenciesFromReceipt(Target.Receipt, Target.RequireFilesExist, Params.UsePak(SC.StageTargetPlatform));
		}

		// Get the build.properties file
		// this file needs to be treated as a UFS file for casing, but NonUFS for being put into the .pak file
		// @todo: Maybe there should be a new category - UFSNotForPak
		string BuildPropertiesPath = CombinePaths(SC.LocalRoot, "Engine/Build");
		if (SC.StageTargetPlatform.DeployLowerCaseFilenames(true))
		{
			BuildPropertiesPath = BuildPropertiesPath.ToLowerInvariant();
		}
		SC.StageFiles(StagedFileType.NonUFS, BuildPropertiesPath, "Build.version", false, null, null, true);

		// move the UE4Commandline.txt file to the root of the stage
		// this file needs to be treated as a UFS file for casing, but NonUFS for being put into the .pak file
		// @todo: Maybe there should be a new category - UFSNotForPak
		string CommandLineFile = "UE4CommandLine.txt";
		if (SC.StageTargetPlatform.DeployLowerCaseFilenames(true))
		{
			CommandLineFile = CommandLineFile.ToLowerInvariant();
		}
		SC.StageFiles(StagedFileType.NonUFS, GetIntermediateCommandlineDir(SC), CommandLineFile, false, null, "", true, false);

        ConfigCacheIni PlatformGameConfig = ConfigCacheIni.CreateConfigCacheIni(SC.StageTargetPlatform.IniPlatformType, "Game", new DirectoryReference(CommandUtils.GetDirectoryName(Params.RawProjectPath.FullName)));
        var ProjectContentRoot = CombinePaths(SC.ProjectRoot, "Content");
        var StageContentRoot = CombinePaths(SC.RelativeProjectRootForStage, "Content");
        
        if (!Params.CookOnTheFly && !Params.SkipCookOnTheFly) // only stage the UFS files if we are not using cook on the fly
        {


            // Initialize internationalization preset.
            string InternationalizationPreset = null;

            // Use parameters if provided.
            if (string.IsNullOrEmpty(InternationalizationPreset))
            {
                InternationalizationPreset = Params.InternationalizationPreset;
            }

            // Use configuration if otherwise lacking an internationalization preset.
            if (string.IsNullOrEmpty(InternationalizationPreset))
            {
                if (PlatformGameConfig != null)
                {
                    PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "InternationalizationPreset", out InternationalizationPreset);
                }
            }

            // Error if no preset has been provided.
            if (string.IsNullOrEmpty(InternationalizationPreset))
            {
                throw new AutomationException("No internationalization preset was specified for packaging. This will lead to fatal errors when launching. Specify preset via commandline (-I18NPreset=) or project packaging settings (InternationalizationPreset).");
            }

            // Initialize cultures to stage.
            List<string> CulturesToStage = null;

            // Use parameters if provided.
            if (Params.CulturesToCook != null && Params.CulturesToCook.Count > 0)
            {
                CulturesToStage = Params.CulturesToCook;
            }

            // Use configuration if otherwise lacking cultures to stage.
            if (CulturesToStage == null || CulturesToStage.Count == 0)
            {
                if (PlatformGameConfig != null)
                {
                    PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "CulturesToStage", out CulturesToStage);
                }
            }

            // Error if no cultures have been provided.
            if (CulturesToStage == null || CulturesToStage.Count == 0)
            {
                throw new AutomationException("No cultures were specified for cooking and packaging. This will lead to fatal errors when launching. Specify culture codes via commandline (-CookCultures=) or using project packaging settings (+CulturesToStage).");
            }

            // Stage ICU internationalization data from Engine.
            SC.StageFiles(StagedFileType.UFS, CombinePaths(SC.LocalRoot, "Engine", "Content", "Internationalization", InternationalizationPreset), "*", true, null, CombinePaths("Engine", "Content", "Internationalization"), true, !Params.UsePak(SC.StageTargetPlatform));

            // Engine ufs (content)
            SC.StageFiles(StagedFileType.UFS, CombinePaths(SC.LocalRoot, "Engine/Config"), "*", true, null, null, false, !Params.UsePak(SC.StageTargetPlatform)); // TODO: Exclude localization data generation config files.

            foreach (string Culture in CulturesToStage)
            {
                StageLocalizationDataForCulture(SC, Culture, CombinePaths(SC.LocalRoot, "Engine/Content/Localization/Engine"), null, !Params.UsePak(SC.StageTargetPlatform));
            }
			
            // Game ufs (content)
			SC.StageFiles(StagedFileType.UFS, CombinePaths(SC.ProjectRoot), "*.uproject", false, null, CombinePaths(SC.RelativeProjectRootForStage), true, !Params.UsePak(SC.StageTargetPlatform));

			if (SC.StageTargetPlatform.PlatformType != UnrealTargetPlatform.HTML5 && SC.bUseWebsocketNetDriver)
			{
				var EngineIniPath = Path.Combine(SC.ProjectRoot, "Config", "DefaultEngine.ini");
				var IntermediateEngineIniDir = Path.Combine(GetIntermediateCommandlineDir(SC), SC.RelativeProjectRootForStage, "Config");
				Directory.CreateDirectory(IntermediateEngineIniDir);
				var IntermediateEngineIniPath = Path.Combine(IntermediateEngineIniDir, "DefaultEngine.ini");
				List<String> IniLines = new List<String>();
				if (File.Exists(EngineIniPath))
				{
					IniLines = File.ReadAllLines(EngineIniPath).ToList();
				}
				IniLines.Add("[/Script/Engine.GameEngine]");
				IniLines.Add("!NetDriverDefinitions=ClearArray");
				IniLines.Add("+NetDriverDefinitions=(DefName=\"GameNetDriver\", DriverClassName=\"/Script/HTML5Networking.WebSocketNetDriver\", DriverClassNameFallback=\"/Script/HTML5Networking.WebSocketNetDriver\")");
				File.WriteAllLines(IntermediateEngineIniPath, IniLines);
				SC.StageFiles(StagedFileType.UFS, CombinePaths(SC.ProjectRoot, "Config"), "*", true, new string[] {"DefaultEngine.ini"}, CombinePaths(SC.RelativeProjectRootForStage, "Config"), true, !Params.UsePak(SC.StageTargetPlatform)); // TODO: Exclude localization data generation config files.
				SC.StageFiles(StagedFileType.UFS, IntermediateEngineIniDir, "*.ini", true, null, CombinePaths(SC.RelativeProjectRootForStage, "Config"), true, !Params.UsePak(SC.StageTargetPlatform)); // TODO: Exclude localization data generation config files.
			}
			else
			{
				SC.StageFiles(StagedFileType.UFS, CombinePaths(SC.ProjectRoot, "Config"), "*", true, null, CombinePaths(SC.RelativeProjectRootForStage, "Config"), true, !Params.UsePak(SC.StageTargetPlatform)); // TODO: Exclude localization data generation config files.
			}

			// Stage all project localization targets
			{
				var ProjectLocRootDirectory = CombinePaths(SC.ProjectRoot, "Content/Localization");
				if (DirectoryExists(ProjectLocRootDirectory))
				{
					string[] ProjectLocTargetDirectories = CommandUtils.FindDirectories(true, "*", false, new string[] { ProjectLocRootDirectory });
					foreach (string ProjectLocTargetDirectory in ProjectLocTargetDirectories)
					{
						foreach (string Culture in CulturesToStage)
						{
							StageLocalizationDataForCulture(SC, Culture, ProjectLocTargetDirectory, null, !Params.UsePak(SC.StageTargetPlatform));
						}
					}
				}
			}

			// Stage all plugin localization targets
			{
				ProjectDescriptor Project = ProjectDescriptor.FromFile(SC.RawProjectPath.FullName);

				List<PluginInfo> AvailablePlugins = Plugins.ReadAvailablePlugins(new DirectoryReference(CombinePaths(SC.LocalRoot, "Engine")), new FileReference(CombinePaths(SC.ProjectRoot, Params.ShortProjectName + ".uproject")));
				foreach (var Plugin in AvailablePlugins)
				{
					if (!UProjectInfo.IsPluginEnabledForProject(Plugin, Project, SC.StageTargetPlatform.PlatformType, TargetRules.TargetType.Game) &&
						UProjectInfo.IsPluginEnabledForProject(Plugin, Project, SC.StageTargetPlatform.PlatformType, TargetRules.TargetType.Client))
					{
						// skip editor plugins
						continue;
					}

					if (Plugin.Descriptor.LocalizationTargets == null || Plugin.Descriptor.LocalizationTargets.Length == 0)
					{
						// skip plugins with no localization targets
						continue;
					}

					foreach (var LocalizationTarget in Plugin.Descriptor.LocalizationTargets)
					{
						if (LocalizationTarget.LoadingPolicy != LocalizationTargetDescriptorLoadingPolicy.Always && LocalizationTarget.LoadingPolicy != LocalizationTargetDescriptorLoadingPolicy.Game)
						{
							// skip targets not loaded by the game
							continue;
						}

						var PluginLocTargetDirectory = CombinePaths(Plugin.Directory.FullName, "Content", "Localization", LocalizationTarget.Name);
						if (DirectoryExists(PluginLocTargetDirectory))
						{
							foreach (string Culture in CulturesToStage)
							{
								StageLocalizationDataForCulture(SC, Culture, PluginLocTargetDirectory, null, !Params.UsePak(SC.StageTargetPlatform));
							}
						}
					}
				}
			}

            // Stage any additional UFS and NonUFS paths specified in the project ini files; these dirs are relative to the game content directory
            if (PlatformGameConfig != null)
            {
                List<string> ExtraUFSDirs;
                if (PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "DirectoriesToAlwaysStageAsUFS", out ExtraUFSDirs))
                {
                    // Each string has the format '(Path="TheDirToStage")'
                    foreach (var PathStr in ExtraUFSDirs)
                    {
                        var PathParts = PathStr.Split('"');
                        if (PathParts.Length == 3)
                        {
                            var RelativePath = PathParts[1];
                            SC.StageFiles(StagedFileType.UFS, CombinePaths(ProjectContentRoot, RelativePath), "*", true, null, CombinePaths(StageContentRoot, RelativePath), true, !Params.UsePak(SC.StageTargetPlatform));
                        }
                        else if (PathParts.Length == 1)
                        {
                            var RelativePath = PathParts[0];
                            SC.StageFiles(StagedFileType.UFS, CombinePaths(ProjectContentRoot, RelativePath), "*", true, null, CombinePaths(StageContentRoot, RelativePath), true, !Params.UsePak(SC.StageTargetPlatform));
                        }
                    }
                }

                List<string> ExtraNonUFSDirs;
                if (PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "DirectoriesToAlwaysStageAsNonUFS", out ExtraNonUFSDirs))
                {
                    // Each string has the format '(Path="TheDirToStage")'
					// NonUFS files are never in pak files and should always be remapped
                    foreach (var PathStr in ExtraNonUFSDirs)
                    {
						var PathParts = PathStr.Split('"');
						if (PathParts.Length == 3)
						{
							var RelativePath = PathParts[1];
							SC.StageFiles(StagedFileType.NonUFS, CombinePaths(ProjectContentRoot, RelativePath), "*", true, null, CombinePaths(StageContentRoot, RelativePath), true, true);
						}
						else if (PathParts.Length == 1)
						{
							var RelativePath = PathParts[0];
							SC.StageFiles(StagedFileType.NonUFS, CombinePaths(ProjectContentRoot, RelativePath), "*", true, null, CombinePaths(StageContentRoot, RelativePath), true, true);
						}
                    }
                }
            }

            StagedFileType StagedFileTypeForMovies = StagedFileType.NonUFS;
            if (Params.FileServer)
            {
                // UFS is required when using a file server
                StagedFileTypeForMovies = StagedFileType.UFS;
            }

            if (SC.StageTargetPlatform.StageMovies && !SC.DedicatedServer)
            {
				if(SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.IOS || SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.TVOS)
				{
					SC.StageFiles(StagedFileTypeForMovies, CombinePaths(SC.LocalRoot, "Engine/Content/Movies"), "*", true, new string[] { "*.uasset", "*.umap" }, CombinePaths(SC.RelativeProjectRootForStage, "Engine/Content/Movies"), true, !Params.UsePak(SC.StageTargetPlatform), null, true, true, SC.StageTargetPlatform.DeployLowerCaseFilenames(true));
					SC.StageFiles(StagedFileTypeForMovies, CombinePaths(SC.ProjectRoot, "Content/Movies"), "*", true, new string[] { "*.uasset", "*.umap" }, CombinePaths(SC.RelativeProjectRootForStage, "Content/Movies"), true, !Params.UsePak(SC.StageTargetPlatform), null, true, true, SC.StageTargetPlatform.DeployLowerCaseFilenames(true));
				}
				else
				{
					SC.StageFiles(StagedFileTypeForMovies, CombinePaths(SC.LocalRoot, "Engine/Content/Movies"), "*", true, new string[] { "*.uasset", "*.umap" }, CombinePaths(SC.RelativeProjectRootForStage, "Engine/Content/Movies"), true, !Params.UsePak(SC.StageTargetPlatform));
					SC.StageFiles(StagedFileTypeForMovies, CombinePaths(SC.ProjectRoot, "Content/Movies"), "*", true, new string[] { "*.uasset", "*.umap" }, CombinePaths(SC.RelativeProjectRootForStage, "Content/Movies"), true, !Params.UsePak(SC.StageTargetPlatform));
				}
            }

            // eliminate the sand box
            SC.StageFiles(StagedFileType.UFS, CombinePaths(SC.ProjectRoot, "Saved", "Cooked", SC.CookPlatform), "*", true, new string[] { "*.json" }, "", true, !Params.UsePak(SC.StageTargetPlatform));

            // CrashReportClient is a standalone slate app that does not look in the generated pak file, so it needs the Content/Slate and Shaders/StandaloneRenderer folders Non-UFS
            // @todo Make CrashReportClient more portable so we don't have to do this
            if (SC.bStageCrashReporter && UnrealBuildTool.UnrealBuildTool.PlatformSupportsCrashReporter(SC.StageTargetPlatform.PlatformType))
            {
                //If the .dat file needs to be staged as NonUFS for non-Windows/Linux hosts we need to change the casing as we do with the build properties file above.
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Content/Slate"));
                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Shaders/StandaloneRenderer"));

                SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine", "Content", "Internationalization", InternationalizationPreset), "*", true, null, CombinePaths("Engine", "Content", "Internationalization"), false, true);

                // Get the architecture in use
                string Architecture = Params.SpecifiedArchitecture;
                if (string.IsNullOrEmpty(Architecture))
                {
                    Architecture = "";
                    var BuildPlatform = UEBuildPlatform.GetBuildPlatform(SC.StageTargetPlatform.PlatformType, true);
                    if (BuildPlatform != null)
                    {
                        Architecture = BuildPlatform.CreateContext(Params.RawProjectPath).GetActiveArchitecture();
                    }
                }

                // Get the target receipt path for CrashReportClient
                DirectoryReference EngineDir = new DirectoryReference(CombinePaths(SC.LocalRoot, "Engine"));
                string ReceiptFileName = TargetReceipt.GetDefaultPath(EngineDir.FullName, "CrashReportClient", SC.StageTargetPlatform.PlatformType, UnrealTargetConfiguration.Shipping, Architecture);
                if (!File.Exists(ReceiptFileName))
                {
                    throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Missing receipt '{0}'. Check that this target has been built.", Path.GetFileName(ReceiptFileName));
                }

                // Read the receipt for this target
                TargetReceipt Receipt;
                if (!TargetReceipt.TryRead(ReceiptFileName, out Receipt))
                {
                    throw new AutomationException("Missing or invalid target receipt ({0})", ReceiptFileName);
                }

                // Stage any runtime dependencies for CrashReportClient
                Receipt.ExpandPathVariables(EngineDir, EngineDir);
                SC.StageRuntimeDependenciesFromReceipt(Receipt, true, Params.UsePak(SC.StageTargetPlatform));

				// Add config files.
				SC.StageFiles( StagedFileType.NonUFS, CombinePaths( SC.LocalRoot, "Engine/Programs/CrashReportClient/Config" ) );
			}
        }
        else
        {
            if (PlatformGameConfig != null)
            {
                List<string> ExtraNonUFSDirs;
                if (PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "DirectoriesToAlwaysStageAsNonUFS", out ExtraNonUFSDirs))
                {
                    // Each string has the format '(Path="TheDirToStage")'
                    foreach (var PathStr in ExtraNonUFSDirs)
                    {
                        var PathParts = PathStr.Split('"');
                        if (PathParts.Length == 3)
                        {
                            var RelativePath = PathParts[1];
                            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(ProjectContentRoot, RelativePath));
                        }
                        else if (PathParts.Length == 1)
                        {
                            var RelativePath = PathParts[0];
                            SC.StageFiles(StagedFileType.UFS, CombinePaths(ProjectContentRoot, RelativePath), "*", true, null, CombinePaths(StageContentRoot, RelativePath), true, !Params.UsePak(SC.StageTargetPlatform));
                        }
                    }
                }
            }
        }
	}

	public static void DumpTargetManifest(Dictionary<string, string> Mapping, string Filename, string StageDir, List<string> CRCFiles)
	{
        // const string Iso8601DateTimeFormat = "yyyy-MM-ddTHH:mm:ssZ"; // probably should work
		// const string Iso8601DateTimeFormat = "o"; // predefined universal Iso standard format (has too many millisecond spaces for our read code in FDateTime.ParseISO8601
        const string Iso8601DateTimeFormat = "yyyy'-'MM'-'dd'T'HH':'mm':'ss'.'fffZ";

        
        if (Mapping.Count > 0)
		{
			var Lines = new List<string>();
			foreach (var Pair in Mapping)
			{
                string TimeStamp = File.GetLastWriteTimeUtc(Pair.Key).ToString(Iso8601DateTimeFormat);
				if (CRCFiles.Contains(Pair.Value))
				{
					byte[] FileData = File.ReadAllBytes(StageDir + "/" + Pair.Value);
					TimeStamp = BitConverter.ToString(System.Security.Cryptography.MD5.Create().ComputeHash(FileData)).Replace("-", string.Empty);
				}
				string Dest = Pair.Value + "\t" + TimeStamp;

				Lines.Add(Dest);
			}
			WriteAllLines(Filename, Lines.ToArray());
		}
	}

	public static void DumpTargetManifest(Dictionary<string, List<string>> Mapping, string Filename, string StageDir, List<string> CRCFiles)
	{
		// const string Iso8601DateTimeFormat = "yyyy-MM-ddTHH:mm:ssZ"; // probably should work
		// const string Iso8601DateTimeFormat = "o"; // predefined universal Iso standard format (has too many millisecond spaces for our read code in FDateTime.ParseISO8601
		const string Iso8601DateTimeFormat = "yyyy'-'MM'-'dd'T'HH':'mm':'ss'.'fffZ";

		if (Mapping.Count > 0)
		{
			var Lines = new List<string>();
			foreach (var Pair in Mapping)
			{
				foreach (var DestFile in Pair.Value)
				{
					string TimeStamp = File.GetLastWriteTimeUtc(Pair.Key).ToString(Iso8601DateTimeFormat);
					if (CRCFiles.Contains(DestFile))
					{
						byte[] FileData = File.ReadAllBytes(StageDir + "/" + DestFile);
						TimeStamp = BitConverter.ToString(System.Security.Cryptography.MD5.Create().ComputeHash(FileData)).Replace("-", string.Empty);
					}
					string DestData = DestFile + "\t" + TimeStamp;
					Lines.Add(DestData);
				}
			}
			WriteAllLines(Filename, Lines.ToArray());
		}
	}

	public static void CopyManifestFilesToStageDir(Dictionary<string, string> Mapping, string StageDir, string ManifestName, List<string> CRCFiles, string PlatformName)
	{
		Log("Copying {0} to staging directory: {1}", ManifestName, StageDir);
		string ManifestPath = "";
		string ManifestFile = "";
		if (!String.IsNullOrEmpty(ManifestName))
		{
			ManifestFile = "Manifest_" + ManifestName + "_" + PlatformName + ".txt";
			ManifestPath = CombinePaths(StageDir, ManifestFile);
			DeleteFile(ManifestPath);
		}
		if (!String.IsNullOrEmpty(ManifestPath) && Mapping.Count > 0)
		{
			DumpTargetManifest(Mapping, ManifestPath, StageDir, CRCFiles);
			if (!FileExists(ManifestPath))
			{
				throw new AutomationException("Failed to write manifest {0}", ManifestPath);
			}
			CopyFile(ManifestPath, CombinePaths(CmdEnv.LogFolder, ManifestFile));
		}
		foreach (var Pair in Mapping)
		{
			string Src = Pair.Key;
			string Dest = CombinePaths(StageDir, Pair.Value);
			if (Src != Dest)  // special case for things created in the staging directory, like the pak file
			{
				CopyFileIncremental(Src, Dest, bFilterSpecialLinesFromIniFiles:true);
			}
		}
	}

	public static void CopyManifestFilesToStageDir(Dictionary<string, List<string>> Mapping, string StageDir, string ManifestName, List<string> CRCFiles, string PlatformName)
	{
		Log("Copying {0} to staging directory: {1}", ManifestName, StageDir);
		string ManifestPath = "";
		string ManifestFile = "";
		if (!String.IsNullOrEmpty(ManifestName))
		{
			ManifestFile = "Manifest_" + ManifestName + "_" + PlatformName + ".txt";
			ManifestPath = CombinePaths(StageDir, ManifestFile);
			DeleteFile(ManifestPath);
		}
		foreach (var Pair in Mapping)
		{
			string Src = Pair.Key;
			foreach (var DestPath in Pair.Value)
			{
				string DestFile = CombinePaths(StageDir, DestPath);
				if (Src != DestFile)  // special case for things created in the staging directory, like the pak file
				{
					CopyFileIncremental(Src, DestFile, bFilterSpecialLinesFromIniFiles: true);
				}
			}
		}
		if (!String.IsNullOrEmpty(ManifestPath) && Mapping.Count > 0)
		{
			DumpTargetManifest(Mapping, ManifestPath, StageDir, CRCFiles);
			if (!FileExists(ManifestPath))
			{
				throw new AutomationException("Failed to write manifest {0}", ManifestPath);
			}
			CopyFile(ManifestPath, CombinePaths(CmdEnv.LogFolder, ManifestFile));
		}
	}

	public static void DumpManifest(Dictionary<string, string> Mapping, string Filename)
	{
		if (Mapping.Count > 0)
		{
			var Lines = new List<string>();
			foreach (var Pair in Mapping)
			{
				string Src = Pair.Key;
				string Dest = Pair.Value;

				Lines.Add("\"" + Src + "\" \"" + Dest + "\"");
			}
			WriteAllLines(Filename, Lines.ToArray());
		}
	}

	public static void DumpManifest(Dictionary<string, List<string>> Mapping, string Filename)
	{
		if (Mapping.Count > 0)
		{
			var Lines = new List<string>();
			foreach (var Pair in Mapping)
			{
				string Src = Pair.Key;
				foreach (var DestPath in Pair.Value)
				{
					Lines.Add("\"" + Src + "\" \"" + DestPath + "\"");
				}
			}
			WriteAllLines(Filename, Lines.ToArray());
		}
	}

	public static void DumpManifest(DeploymentContext SC, string BaseFilename, bool DumpUFSFiles = true)
	{
		DumpManifest(SC.NonUFSStagingFiles, BaseFilename + "_NonUFSFiles.txt");
		if (DumpUFSFiles)
		{
			DumpManifest(SC.NonUFSStagingFilesDebug, BaseFilename + "_NonUFSFilesDebug.txt");
		}
		DumpManifest(SC.UFSStagingFiles, BaseFilename + "_UFSFiles.txt");
	}

	public static void CopyUsingStagingManifest(ProjectParams Params, DeploymentContext SC)
	{
		CopyManifestFilesToStageDir(SC.NonUFSStagingFiles, SC.StageDirectory, "NonUFSFiles", SC.StageTargetPlatform.GetFilesForCRCCheck(), SC.StageTargetPlatform.PlatformType.ToString());

		if (!Params.NoDebugInfo)
		{
			CopyManifestFilesToStageDir(SC.NonUFSStagingFilesDebug, SC.StageDirectory, "DebugFiles", SC.StageTargetPlatform.GetFilesForCRCCheck(), SC.StageTargetPlatform.PlatformType.ToString());
		}

		bool bStageUnrealFileSystemFiles = !Params.CookOnTheFly && !Params.UsePak(SC.StageTargetPlatform) && !Params.FileServer;
		if (bStageUnrealFileSystemFiles)
		{
			CopyManifestFilesToStageDir(SC.UFSStagingFiles, SC.StageDirectory, "UFSFiles", SC.StageTargetPlatform.GetFilesForCRCCheck(), SC.StageTargetPlatform.PlatformType.ToString());
		}
	}

	/// <summary>
	/// Creates a pak file using staging context (single manifest)
	/// </summary>
	/// <param name="Params"></param>
	/// <param name="SC"></param>
	private static void CreatePakUsingStagingManifest(ProjectParams Params, DeploymentContext SC)
	{
		Log("Creating pak using staging manifest.");

		DumpManifest(SC, CombinePaths(CmdEnv.LogFolder, "PrePak" + (SC.DedicatedServer ? "_Server" : "")));

		var UnrealPakResponseFile = CreatePakResponseFileFromStagingManifest(SC);

		CreatePak(Params, SC, UnrealPakResponseFile, SC.ShortProjectName, Params.SignPak);
	}

	/// <summary>
	/// Creates a pak response file using stage context
	/// </summary>
	/// <param name="SC"></param>
	/// <returns></returns>
	private static Dictionary<string, string> CreatePakResponseFileFromStagingManifest(DeploymentContext SC)
	{
		// look for optional packaging blacklist if only one config active
		List<string> Blacklist = null;
		if (SC.StageTargetConfigurations.Count == 1)
		{
			var PakBlacklistFilename = CombinePaths(SC.ProjectRoot, "Build", SC.PlatformDir, string.Format("PakBlacklist-{0}.txt", SC.StageTargetConfigurations[0].ToString()));
			if (File.Exists(PakBlacklistFilename))
			{
				Log("Applying PAK blacklist file {0}", PakBlacklistFilename);
				string[] BlacklistContents = File.ReadAllLines(PakBlacklistFilename);
				foreach (string Candidate in BlacklistContents)
				{
					if (Candidate.Trim().Length > 0)
					{
						if (Blacklist == null)
						{
							Blacklist = new List<string>();
						}
						Blacklist.Add(Candidate);
					}
				}
			}
		}

		var UnrealPakResponseFile = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);
		foreach (var Pair in SC.UFSStagingFiles)
		{
			string Src = Pair.Key;
			string Dest = Pair.Value;

			Dest = CombinePaths(PathSeparator.Slash, SC.PakFileInternalRoot, Dest);

			if (Blacklist != null)
			{
				bool bExcludeFile = false;
				foreach (string ExcludePath in Blacklist)
				{
					if (Dest.StartsWith(ExcludePath))
					{
						bExcludeFile = true;
						break;
					}
				}

				if (bExcludeFile)
				{
					Log("Excluding {0}", Src);
					continue;
				}
			}

			// special case DefaultEngine.ini to strip passwords
			if (Path.GetFileName(Src).Equals("DefaultEngine.ini"))
			{
				string SrcDirectoryPath = Path.GetDirectoryName(Src);
				string ConfigDirectory = "Config";
				int ConfigRootIdx = SrcDirectoryPath.LastIndexOf(ConfigDirectory);
				string SubpathUnderConfig = (ConfigRootIdx != -1) ? SrcDirectoryPath.Substring(ConfigRootIdx + ConfigDirectory.Length) : "Unknown";

				string NewIniFilename = CombinePaths(SC.ProjectRoot, "Saved", "Temp", SC.PlatformDir, SubpathUnderConfig, "DefaultEngine.ini");
				InternalUtils.SafeCreateDirectory(Path.GetDirectoryName(NewIniFilename), true);
				InternalUtils.SafeCopyFile(Src, NewIniFilename, bFilterSpecialLinesFromIniFiles:true);
				Src = NewIniFilename;
			}

            // there can be files that only differ in case only, we don't support that in paks as paks are case-insensitive
            if (UnrealPakResponseFile.ContainsKey(Src))
            {
                if (UnrealPakResponseFile[Src] != Dest)
                {
                    throw new AutomationException("Staging manifest already contains {0} (or a file that differs in case only)", Src);
                }
                LogWarning("Tried to add duplicate file to stage " + Src + " ignoring second attempt pls fix");
                continue;
            }

			UnrealPakResponseFile.Add(Src, Dest);
		}

		return UnrealPakResponseFile;
	}


	/// <summary>
	/// Creates a pak file using response file.
	/// </summary>
	/// <param name="Params"></param>
	/// <param name="SC"></param>
	/// <param name="UnrealPakResponseFile"></param>
	/// <param name="PakName"></param>
	private static void CreatePak(ProjectParams Params, DeploymentContext SC, Dictionary<string, string> UnrealPakResponseFile, string PakName, string EncryptionKeys)
	{
        bool bShouldGeneratePatch = Params.IsGeneratingPatch && SC.StageTargetPlatform.GetPlatformPatchesWithDiffPak(Params, SC);

        if (bShouldGeneratePatch && !Params.HasBasedOnReleaseVersion)
        {
            Log("Generating patch required a based on release version flag");
        }

        string PostFix = "";
		if (bShouldGeneratePatch)
        {
            PostFix += "_P";
        }
		var OutputRelativeLocation = CombinePaths(SC.RelativeProjectRootForStage, "Content/Paks/", PakName + "-" + SC.FinalCookPlatform + PostFix + ".pak");
		if (SC.StageTargetPlatform.DeployLowerCaseFilenames(true))
		{
			OutputRelativeLocation = OutputRelativeLocation.ToLowerInvariant();
		}
		OutputRelativeLocation = SC.StageTargetPlatform.Remap(OutputRelativeLocation);
		var OutputLocation = CombinePaths(SC.RuntimeRootDir, OutputRelativeLocation);
		// Add input file to controll order of file within the pak
		var PakOrderFileLocationBase = CombinePaths(SC.ProjectRoot, "Build", SC.FinalCookPlatform, "FileOpenOrder");

		var OrderFileLocations = new string[] { "GameOpenOrder.log", "CookerOpenOrder.log", "EditorOpenOrder.log" };
		string PakOrderFileLocation = null;
		foreach (var Location in OrderFileLocations)
		{
			PakOrderFileLocation = CombinePaths(PakOrderFileLocationBase, Location);

			if (FileExists_NoExceptions(PakOrderFileLocation))
			{
				break;
			}
		}

		bool bCopiedExistingPak = false;

		if (SC.StageTargetPlatform != SC.CookSourcePlatform)
		{
			// Check to see if we have an existing pak file we can use

			var SourceOutputRelativeLocation = CombinePaths(SC.RelativeProjectRootForStage, "Content/Paks/", PakName + "-" + SC.CookPlatform + PostFix + ".pak");
			if (SC.CookSourcePlatform.DeployLowerCaseFilenames(true))
			{
				SourceOutputRelativeLocation = SourceOutputRelativeLocation.ToLowerInvariant();
			}
			SourceOutputRelativeLocation = SC.CookSourcePlatform.Remap(SourceOutputRelativeLocation);
			var SourceOutputLocation = CombinePaths(SC.CookSourceRuntimeRootDir, SourceOutputRelativeLocation);

			if (FileExists_NoExceptions(SourceOutputLocation))
			{
				InternalUtils.SafeCreateDirectory(Path.GetDirectoryName(OutputLocation), true);

				if (InternalUtils.SafeCopyFile(SourceOutputLocation, OutputLocation))
				{
					Log("Copying source pak from {0} to {1} instead of creating new pak", SourceOutputLocation, OutputLocation);
					bCopiedExistingPak = true;
				}
			}
			if (!bCopiedExistingPak)
			{
				Log("Failed to copy source pak from {0} to {1}, creating new pak", SourceOutputLocation, OutputLocation);
			}
		}

        string PatchSourceContentPath = null;
		if (bShouldGeneratePatch)
        {
            // don't include the post fix in this filename because we are looking for the source pak path
            string PakFilename = PakName + "-" + SC.FinalCookPlatform + ".pak";
            PatchSourceContentPath = SC.StageTargetPlatform.GetReleasePakFilePath(SC, Params, PakFilename);
        }

		ConfigCacheIni PlatformGameConfig = ConfigCacheIni.CreateConfigCacheIni(SC.StageTargetPlatform.IniPlatformType, "Game", new DirectoryReference(CommandUtils.GetDirectoryName(Params.RawProjectPath.FullName)));
		bool PackageSettingsEncryptIniFiles = false;
		PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bEncryptIniFiles", out PackageSettingsEncryptIniFiles);

		if (!bCopiedExistingPak)
		{
			if (File.Exists(OutputLocation))
			{
				string UnrealPakResponseFileName = CombinePaths(CmdEnv.LogFolder, "PakList_" + Path.GetFileNameWithoutExtension(OutputLocation) + ".txt");
				if (File.Exists(UnrealPakResponseFileName) && File.GetLastWriteTimeUtc(OutputLocation) > File.GetLastWriteTimeUtc(UnrealPakResponseFileName))
				{
					bCopiedExistingPak = true;
				}
			}
			if (!bCopiedExistingPak)
			{
				RunUnrealPak(UnrealPakResponseFile, OutputLocation, EncryptionKeys, PakOrderFileLocation, SC.StageTargetPlatform.GetPlatformPakCommandLine(), Params.Compressed, Params.EncryptIniFiles || PackageSettingsEncryptIniFiles, PatchSourceContentPath);
			}
		}


        if (Params.HasCreateReleaseVersion)
        {
            // copy the created pak to the release version directory we might need this later if we want to generate patches
            //string ReleaseVersionPath = CombinePaths( SC.ProjectRoot, "Releases", Params.CreateReleaseVersion, SC.StageTargetPlatform.GetCookPlatform(Params.DedicatedServer, false), Path.GetFileName(OutputLocation) );
            string ReleaseVersionPath = SC.StageTargetPlatform.GetReleasePakFilePath(SC, Params, Path.GetFileName(OutputLocation));

			InternalUtils.SafeCreateDirectory(Path.GetDirectoryName(ReleaseVersionPath));
			InternalUtils.SafeCopyFile(OutputLocation, ReleaseVersionPath);
        }

		if (Params.CreateChunkInstall)
		{
            var RegEx = new Regex("pakchunk(\\d+)", RegexOptions.IgnoreCase | RegexOptions.Compiled);
            var Matches = RegEx.Matches(PakName);

            int ChunkID = 0;
            if (Matches.Count != 0 && Matches[0].Groups.Count > 1)
            {
                ChunkID = Convert.ToInt32(Matches[0].Groups[1].ToString());
            }
            else if (Params.HasDLCName) 
            {
                // Assuming DLC is a single pack file
                ChunkID = 1; 
            }
            else
            {
                throw new AutomationException(String.Format("Failed Creating Chunk Install Data, Unable to parse chunk id from {0}", PakName));
            }

            if (ChunkID != 0)
			{
                var BPTExe = GetBuildPatchToolExecutable();
				EnsureBuildPatchToolExists();

				string VersionString = Params.ChunkInstallVersionString;
				string ChunkInstallBasePath = CombinePaths(Params.ChunkInstallDirectory, SC.FinalCookPlatform);
				string RawDataPath = CombinePaths(ChunkInstallBasePath, VersionString, PakName);
				string RawDataPakPath = CombinePaths(RawDataPath, PakName + "-" + SC.FinalCookPlatform + PostFix + ".pak");
				//copy the pak chunk to the raw data folder
				if (InternalUtils.SafeFileExists(RawDataPakPath, true))
				{
					InternalUtils.SafeDeleteFile(RawDataPakPath, true);
				}
				InternalUtils.SafeCreateDirectory(RawDataPath, true);
				InternalUtils.SafeCopyFile(OutputLocation, RawDataPakPath);
				InternalUtils.SafeDeleteFile(OutputLocation, true);

                if (Params.IsGeneratingPatch)
				{
					if (String.IsNullOrEmpty(PatchSourceContentPath))
					{
						throw new AutomationException(String.Format("Failed Creating Chunk Install Data. No source pak for patch pak {0} given", OutputLocation));
					}
					// If we are generating a patch, then we need to copy the original pak across
					// for distribution.
					string SourceRawDataPakPath = CombinePaths(RawDataPath, PakName + "-" + SC.FinalCookPlatform + ".pak");
					InternalUtils.SafeCopyFile(PatchSourceContentPath, SourceRawDataPakPath);
				}

				string BuildRoot = MakePathSafeToUseWithCommandLine(RawDataPath);
				string CloudDir = MakePathSafeToUseWithCommandLine(CombinePaths(ChunkInstallBasePath, "CloudDir"));
                InternalUtils.SafeDeleteDirectory(CloudDir, true);
				string ManifestDir = CombinePaths(ChunkInstallBasePath, "ManifestDir");
				var AppID = 1; // For a chunk install this value doesn't seem to matter
				string AppName = String.Format("{0}_{1}", SC.ShortProjectName, PakName);
				string AppLaunch = ""; // For a chunk install this value doesn't seem to matter
				string ManifestFilename = AppName + VersionString + ".manifest";
				string SourceManifestPath = CombinePaths(CloudDir, ManifestFilename);
				string BackupManifestPath = CombinePaths(RawDataPath, ManifestFilename);
				string DestManifestPath = CombinePaths(ManifestDir, ManifestFilename);
				InternalUtils.SafeCreateDirectory(ManifestDir, true);

				string CmdLine = String.Format("-BuildRoot=\"{0}\" -CloudDir=\"{1}\" -AppID={2} -AppName=\"{3}\" -BuildVersion=\"{4}\" -AppLaunch=\"{5}\"", BuildRoot, CloudDir, AppID, AppName, VersionString, AppLaunch);
				CmdLine += " -AppArgs=\"\"";
				CmdLine += " -custom=\"bIsPatch=false\"";
				CmdLine += String.Format(" -customint=\"ChunkID={0}\"", ChunkID);
				CmdLine += " -customint=\"PakReadOrdering=0\"";
				CmdLine += " -stdout";

				string UnrealPakLogFileName = "UnrealPak_" + PakName;
				RunAndLog(CmdEnv, BPTExe, CmdLine, UnrealPakLogFileName, Options: ERunOptions.Default | ERunOptions.UTF8Output);

				InternalUtils.SafeCopyFile(SourceManifestPath, BackupManifestPath);
				InternalUtils.SafeCopyFile(SourceManifestPath, DestManifestPath);
			}
			else
			{
				// add the first pak file as needing deployment and convert to lower case again if needed
				SC.UFSStagingFiles.Add(OutputLocation, OutputRelativeLocation);
			}
		}
		else
		{
			// add the pak file as needing deployment and convert to lower case again if needed
			SC.UFSStagingFiles.Add(OutputLocation, OutputRelativeLocation);
		}
	}

	/// <summary>
	/// Creates pak files using streaming install chunk manifests.
	/// </summary>
	/// <param name="Params"></param>
	/// <param name="SC"></param>
	private static void CreatePaksUsingChunkManifests(ProjectParams Params, DeploymentContext SC)
	{
		Log("Creating pak using streaming install manifests.");
		DumpManifest(SC, CombinePaths(CmdEnv.LogFolder, "PrePak" + (SC.DedicatedServer ? "_Server" : "")));

		var TmpPackagingPath = GetTmpPackagingPath(Params, SC);
		var ChunkListFilename = GetChunkPakManifestListFilename(Params, SC);
		var ChunkList = ReadAllLines(ChunkListFilename);
		var ChunkResponseFiles = new HashSet<string>[ChunkList.Length];
		for (int Index = 0; Index < ChunkList.Length; ++Index)
		{
			var ChunkManifestFilename = CombinePaths(TmpPackagingPath, ChunkList[Index]);
			ChunkResponseFiles[Index] = ReadPakChunkManifest(ChunkManifestFilename);
		}
		// We still want to have a list of all files to stage. We will use the chunk manifests
		// to put the files from staging manigest into the right chunk
		var StagingManifestResponseFile = CreatePakResponseFileFromStagingManifest(SC);
		// DefaultChunkIndex assumes 0 is the 'base' chunk
		const int DefaultChunkIndex = 0;
		var PakResponseFiles = new Dictionary<string, string>[ChunkList.Length];
		for (int Index = 0; Index < PakResponseFiles.Length; ++Index)
		{
			PakResponseFiles[Index] = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);
		}
		foreach (var StagingFile in StagingManifestResponseFile)
		{
			bool bAddedToChunk = false;
			for (int ChunkIndex = 0; !bAddedToChunk && ChunkIndex < ChunkResponseFiles.Length; ++ChunkIndex)
			{
                		string OriginalFilename = StagingFile.Key;
                		string NoExtension = CombinePaths(Path.GetDirectoryName(OriginalFilename), Path.GetFileNameWithoutExtension(OriginalFilename));
                		string OriginalReplaceSlashes = OriginalFilename.Replace('/', '\\');
                		string NoExtensionReplaceSlashes = NoExtension.Replace('/', '\\');

				if (ChunkResponseFiles[ChunkIndex].Contains(OriginalFilename) || 
                		    ChunkResponseFiles[ChunkIndex].Contains(OriginalReplaceSlashes) ||
		                    ChunkResponseFiles[ChunkIndex].Contains(NoExtension) ||
		                    ChunkResponseFiles[ChunkIndex].Contains(NoExtensionReplaceSlashes))
				{
					PakResponseFiles[ChunkIndex].Add(StagingFile.Key, StagingFile.Value);
					bAddedToChunk = true;
				}
			}
			if (!bAddedToChunk)
			{
				//Log("No chunk assigned found for {0}. Using default chunk.", StagingFile.Key);
				PakResponseFiles[DefaultChunkIndex].Add(StagingFile.Key, StagingFile.Value);
			}
		}

		if (Params.CreateChunkInstall)
		{
			string ManifestDir = CombinePaths(Params.ChunkInstallDirectory, SC.FinalCookPlatform, "ManifestDir");
			if (InternalUtils.SafeDirectoryExists(ManifestDir))
			{
				foreach (string ManifestFile in Directory.GetFiles(ManifestDir, "*.manifest"))
				{
					InternalUtils.SafeDeleteFile(ManifestFile, true);
				}
			}
			string DestDir = CombinePaths(Params.ChunkInstallDirectory, SC.FinalCookPlatform, Params.ChunkInstallVersionString);
			if (InternalUtils.SafeDirectoryExists(DestDir))
			{
				InternalUtils.SafeDeleteDirectory(DestDir); 
			}
		}

		IEnumerable<Tuple<Dictionary<string,string>, string>> PakPairs = PakResponseFiles.Zip(ChunkList, (a, b) => Tuple.Create(a, b));

		System.Threading.Tasks.Parallel.ForEach(PakPairs, (PakPair) =>
		{
			var ChunkName = Path.GetFileNameWithoutExtension(PakPair.Item2);
			CreatePak(Params, SC, PakPair.Item1, ChunkName, Params.SignPak);
		});

		String ChunkLayerFilename = CombinePaths(GetTmpPackagingPath(Params, SC), GetChunkPakLayerListName());
		String OutputChunkLayerFilename = Path.Combine(SC.ProjectRoot, "Build", SC.FinalCookPlatform, "ChunkLayerInfo", GetChunkPakLayerListName());
		Directory.CreateDirectory(Path.GetDirectoryName(OutputChunkLayerFilename));
		File.Copy(ChunkLayerFilename, OutputChunkLayerFilename, true);
	}

	private static bool DoesChunkPakManifestExist(ProjectParams Params, DeploymentContext SC)
	{
		return FileExists_NoExceptions(GetChunkPakManifestListFilename(Params, SC));
	}

	private static string GetChunkPakManifestListFilename(ProjectParams Params, DeploymentContext SC)
	{
		return CombinePaths(GetTmpPackagingPath(Params, SC), "pakchunklist.txt");
	}

	private static string GetChunkPakLayerListName()
	{
		return "pakchunklayers.txt";
	}

	private static string GetTmpPackagingPath(ProjectParams Params, DeploymentContext SC)
	{
		return CombinePaths(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Saved", "TmpPackaging", SC.StageTargetPlatform.GetCookPlatform(SC.DedicatedServer, false));
	}

	private static bool ShouldCreatePak(ProjectParams Params, DeploymentContext SC)
	{
        Platform.PakType Pak = SC.StageTargetPlatform.RequiresPak(Params);

        // we may care but we don't want. 
        if (Params.SkipPak)
            return false; 

        if (Pak == Platform.PakType.Always)
        {
            return true;
        }
        else if (Pak == Platform.PakType.Never)
        {
            return false;
        }
        else // DontCare
        {
            return (Params.Pak);
        }
	}

	private static bool ShouldCreatePak(ProjectParams Params)
	{
        Platform.PakType Pak = Params.ClientTargetPlatformInstances[0].RequiresPak(Params);

        // we may care but we don't want. 
        if (Params.SkipPak)
            return false; 

        if (Pak == Platform.PakType.Always)
        {
            return true;
        }
        else if (Pak == Platform.PakType.Never)
        {
            return false;
        }
        else // DontCare
        {
            return (Params.Pak);
        }
	}

	protected static void DeletePakFiles(string StagingDirectory)
	{
		var StagedFilesDir = new DirectoryInfo(StagingDirectory);
		StagedFilesDir.GetFiles("*.pak", SearchOption.AllDirectories).ToList().ForEach(File => File.Delete());
	}

	public static void CleanStagingDirectory(ProjectParams Params, DeploymentContext SC)
	{
		MaybeConvertToLowerCase(Params, SC);
		Log("Cleaning Stage Directory: {0}", SC.StageDirectory);
		if (SC.Stage && !Params.NoCleanStage && !Params.SkipStage && !Params.IterativeDeploy)
		{
            try
            {
                DeleteDirectory(SC.StageDirectory);
            }
            catch (Exception Ex)
            {
                // Delete cooked data (if any) as it may be incomplete / corrupted.
                throw new AutomationException(ExitCode.Error_FailedToDeleteStagingDirectory, Ex, "Stage Failed. Failed to delete staging directory " + SC.StageDirectory);
            }
		}
		else
		{
			try
			{
				// delete old pak files
				DeletePakFiles(SC.StageDirectory);
			}
			catch (Exception Ex)
			{
				// Delete cooked data (if any) as it may be incomplete / corrupted.
				throw new AutomationException(ExitCode.Error_FailedToDeleteStagingDirectory, Ex, "Stage Failed. Failed to delete pak files in " + SC.StageDirectory);
			}
		}
	}

	public static void ApplyStagingManifest(ProjectParams Params, DeploymentContext SC)
	{
		if (ShouldCreatePak(Params, SC))
		{
			if (Params.Manifests && DoesChunkPakManifestExist(Params, SC))
			{
				CreatePaksUsingChunkManifests(Params, SC);
			}
			else
			{
				CreatePakUsingStagingManifest(Params, SC);
			}
		}
		if (!SC.Stage || Params.SkipStage)
		{
			return;
		}
		DumpManifest(SC, CombinePaths(CmdEnv.LogFolder, "FinalCopy" + (SC.DedicatedServer ? "_Server" : ""))/*, !Params.UsePak(SC.StageTargetPlatform)*/);
		CopyUsingStagingManifest(Params, SC);

		var ThisPlatform = SC.StageTargetPlatform;
		ThisPlatform.PostStagingFileCopy(Params, SC);
	}

	private static string GetIntermediateCommandlineDir(DeploymentContext SC)
	{
		return CombinePaths(SC.ProjectRoot, "Intermediate/UAT", SC.FinalCookPlatform);
	}

	public static void WriteStageCommandline(string IntermediateCmdLineFile, ProjectParams Params, DeploymentContext SC)
	{
		// this file needs to be treated as a UFS file for casing, but NonUFS for being put into the .pak file. 
		// @todo: Maybe there should be a new category - UFSNotForPak
		if (SC.StageTargetPlatform.DeployLowerCaseFilenames(true))
		{
			IntermediateCmdLineFile = IntermediateCmdLineFile.ToLowerInvariant();
		}
		if (File.Exists(IntermediateCmdLineFile))
		{
			File.Delete(IntermediateCmdLineFile);
		}

		if(!SC.StageTargetPlatform.ShouldStageCommandLine(Params, SC))
		{
			return;
		}

		Log("Creating UE4CommandLine.txt");
		if (!string.IsNullOrEmpty(Params.StageCommandline) || !string.IsNullOrEmpty(Params.RunCommandline))
		{
			string FileHostParams = " ";
			if (Params.CookOnTheFly || Params.FileServer)
			{
				FileHostParams += "-filehostip=";
				bool FirstParam = true;
				if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					NetworkInterface[] Interfaces = NetworkInterface.GetAllNetworkInterfaces();
					foreach (NetworkInterface adapter in Interfaces)
					{
						if (adapter.NetworkInterfaceType != NetworkInterfaceType.Loopback)
						{
							IPInterfaceProperties IP = adapter.GetIPProperties();
							for (int Index = 0; Index < IP.UnicastAddresses.Count; ++Index)
							{
								if (IP.UnicastAddresses[Index].IsDnsEligible && IP.UnicastAddresses[Index].Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
								{
                                    if (!IsNullOrEmpty(Params.Port))
                                    {
                                        foreach (var Port in Params.Port)
                                        {
                                            if (!FirstParam)
                                            {
                                                FileHostParams += "+";
                                            }
                                            FirstParam = false;
                                            string[] PortProtocol = Port.Split(new char[] { ':' });
                                            if (PortProtocol.Length > 1)
                                            {
                                                FileHostParams += String.Format("{0}://{1}:{2}", PortProtocol[0], IP.UnicastAddresses[Index].Address.ToString(), PortProtocol[1]);
                                            }
                                            else
                                            {
                                                FileHostParams += IP.UnicastAddresses[Index].Address.ToString();
                                                FileHostParams += ":";
                                                FileHostParams += Params.Port;
                                            }

                                        }
                                    }
                                    else
                                    {
										if (!FirstParam)
										{
											FileHostParams += "+";
										}
										FirstParam = false;
										
										// use default port
                                        FileHostParams += IP.UnicastAddresses[Index].Address.ToString();
                                    }

								}
							}
						}
					}
				}
				else
				{
					NetworkInterface[] Interfaces = NetworkInterface.GetAllNetworkInterfaces();
					foreach (NetworkInterface adapter in Interfaces)
					{
						if (adapter.OperationalStatus == OperationalStatus.Up)
						{
							IPInterfaceProperties IP = adapter.GetIPProperties();
							for (int Index = 0; Index < IP.UnicastAddresses.Count; ++Index)
							{
								if (IP.UnicastAddresses[Index].IsDnsEligible)
								{
									if (!IsNullOrEmpty(Params.Port))
									{
										foreach (var Port in Params.Port)
										{
											if (!FirstParam)
											{
												FileHostParams += "+";
											}
											FirstParam = false;
											string[] PortProtocol = Port.Split(new char[] { ':' });
											if (PortProtocol.Length > 1)
											{
												FileHostParams += String.Format("{0}://{1}:{2}", PortProtocol[0], IP.UnicastAddresses[Index].Address.ToString(), PortProtocol[1]);
											}
											else
											{
												FileHostParams += IP.UnicastAddresses[Index].Address.ToString();
												FileHostParams += ":";
												FileHostParams += Params.Port;
											}
										}
									}
                                    else
                                    {
										if (!FirstParam)
										{
											FileHostParams += "+";
										}
										FirstParam = false;
										
										// use default port
                                        FileHostParams += IP.UnicastAddresses[Index].Address.ToString();
                                    }

								}
							}
						}
					}
				}
				const string LocalHost = "127.0.0.1";
				if (!IsNullOrEmpty(Params.Port))
				{
					foreach (var Port in Params.Port)
					{
						if (!FirstParam)
						{
							FileHostParams += "+";
						}
						FirstParam = false;
						string[] PortProtocol = Port.Split(new char[] { ':' });
						if (PortProtocol.Length > 1)
						{
							FileHostParams += String.Format("{0}://{1}:{2}", PortProtocol[0], LocalHost, PortProtocol[1]);
						}
						else
						{
							FileHostParams += LocalHost;
							FileHostParams += ":";
							FileHostParams += Params.Port;
						}

					}
				}
                else
                {
					if (!FirstParam)
					{
						FileHostParams += "+";
					}
					FirstParam = false;
					
					// use default port
                    FileHostParams += LocalHost;
                }
				FileHostParams += " ";
			}

			String ProjectFile = String.Format("{0} ", SC.ProjectArgForCommandLines);
			if (SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Mac || SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Win64 || SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Win32 || SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Linux)
			{
				ProjectFile = "";
			}
			Directory.CreateDirectory(GetIntermediateCommandlineDir(SC));
			string CommandLine = String.Format("{0} {1} {2} {3}\n", ProjectFile, Params.StageCommandline.Trim(new char[] { '\"' }), Params.RunCommandline.Trim(new char[] { '\"' }), FileHostParams).Trim();
			if (Params.IterativeDeploy)
			{
				CommandLine += " -iterative";
			}
			File.WriteAllText(IntermediateCmdLineFile, CommandLine);
		}
		else
		{
			String ProjectFile = String.Format("{0} ", SC.ProjectArgForCommandLines);
			if (SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Mac || SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Win64 || SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Win32 || SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Linux)
			{
				ProjectFile = "";
			}
			Directory.CreateDirectory(GetIntermediateCommandlineDir(SC));
			File.WriteAllText(IntermediateCmdLineFile, ProjectFile);
		}
	}

	private static void WriteStageCommandline(ProjectParams Params, DeploymentContext SC)
	{
		// always delete the existing commandline text file, so it doesn't reuse an old one
		string IntermediateCmdLineFile = CombinePaths(GetIntermediateCommandlineDir(SC), "UE4CommandLine.txt");
		WriteStageCommandline(IntermediateCmdLineFile, Params, SC);
	}

	private static Dictionary<string, string> ReadDeployedManifest(ProjectParams Params, DeploymentContext SC, List<string> ManifestList)
	{
		Dictionary<string, string> DeployedFiles = new Dictionary<string, string>();
		List<string> CRCFiles = SC.StageTargetPlatform.GetFilesForCRCCheck();

		// read the manifest
		bool bContinueSearch = true;
		foreach (string Manifest in ManifestList)
		{
			int FilesAdded = 0;
			if (bContinueSearch)
			{
				string Data = File.ReadAllText (Manifest);
				string[] Lines = Data.Split ('\n');
				foreach (string Line in Lines)
				{
					string[] Pair = Line.Split ('\t');
					if (Pair.Length > 1)
					{
						string Filename = Pair [0];
						string TimeStamp = Pair [1];
						FilesAdded++;
						if (DeployedFiles.ContainsKey (Filename))
						{
							if ((CRCFiles.Contains (Filename) && DeployedFiles [Filename] != TimeStamp) || (!CRCFiles.Contains (Filename) && DateTime.Parse (DeployedFiles [Filename]) > DateTime.Parse (TimeStamp)))
							{
								DeployedFiles [Filename] = TimeStamp;
							}
						}
						else
						{
							DeployedFiles.Add (Filename, TimeStamp);
						}
					}
				}
			}
			File.Delete(Manifest);

			if (FilesAdded == 0 && bContinueSearch)
			{
				// no files have been deployed at all to this guy, so remove all previously added files and exit the loop as this means we need to deploy everything
				DeployedFiles.Clear ();
				bContinueSearch = false;
			}
		}

		return DeployedFiles;
	}

	protected static Dictionary<string, string> ReadStagedManifest(ProjectParams Params, DeploymentContext SC, string Manifest)
	{
		Dictionary <string, string> StagedFiles = new Dictionary<string, string>();
		List<string> CRCFiles = SC.StageTargetPlatform.GetFilesForCRCCheck();

		// get the staged manifest from staged directory
		string ManifestFile = SC.StageDirectory + "/" + Manifest;
		if (File.Exists(ManifestFile))
		{
			string Data = File.ReadAllText(ManifestFile);
			string[] Lines = Data.Split('\n');
			foreach (string Line in Lines)
			{
				string[] Pair = Line.Split('\t');
				if (Pair.Length > 1)
				{
					string Filename = Pair[0];
					string TimeStamp = Pair[1];
					if (!StagedFiles.ContainsKey(Filename))
					{
						StagedFiles.Add(Filename, TimeStamp);
					}
					else if ((CRCFiles.Contains(Filename) && StagedFiles[Filename] != TimeStamp) || (!CRCFiles.Contains(Filename) && DateTime.Parse(StagedFiles[Filename]) > DateTime.Parse(TimeStamp)))
					{
						StagedFiles[Filename] = TimeStamp;
					}
				}
			}
		}
		return StagedFiles;
	}

	protected static void WriteObsoleteManifest(ProjectParams Params, DeploymentContext SC, Dictionary<string, string> DeployedFiles, Dictionary<string, string> StagedFiles, string ObsoleteManifest)
	{
		List<string> ObsoleteFiles = new List<string>();

		// any file that has been deployed, but is no longer in the staged files is obsolete and should be deleted.
		foreach (KeyValuePair<string, string> File in DeployedFiles)
		{
			if (!StagedFiles.ContainsKey(File.Key))
			{
				ObsoleteFiles.Add(File.Key);
			}
		}

		// write out to the deltamanifest.json
		string ManifestFile = CombinePaths(SC.StageDirectory, ObsoleteManifest);
		StreamWriter Writer = System.IO.File.CreateText(ManifestFile);
		foreach (string Line in ObsoleteFiles)
		{
			Writer.WriteLine(Line);
		}
		Writer.Close();
	}	

	protected static void WriteDeltaManifest(ProjectParams Params, DeploymentContext SC, Dictionary<string, string> DeployedFiles, Dictionary<string, string> StagedFiles, string DeltaManifest)
	{
		List<string> CRCFiles = SC.StageTargetPlatform.GetFilesForCRCCheck();
		List<string> DeltaFiles = new List<string>();
		foreach (KeyValuePair<string, string> File in StagedFiles)
		{
			bool bNeedsDeploy = true;
			if (DeployedFiles.ContainsKey(File.Key))
			{
				if (CRCFiles.Contains(File.Key))
				{
					bNeedsDeploy = (File.Value != DeployedFiles[File.Key]);
				}
				else
				{
					DateTime Staged = DateTime.Parse(File.Value);
					DateTime Deployed = DateTime.Parse(DeployedFiles[File.Key]);
					bNeedsDeploy = (Staged > Deployed);
				}
			}

			if (bNeedsDeploy)
			{
				DeltaFiles.Add(File.Key);
			}
		}

		// add the manifest
		if (!DeltaManifest.Contains("NonUFS"))
		{
			DeltaFiles.Add(SC.NonUFSDeployedManifestFileName);
			DeltaFiles.Add(SC.UFSDeployedManifestFileName);
		}

		// TODO: determine files which need to be removed

		// write out to the deltamanifest.json
		string ManifestFile = CombinePaths(SC.StageDirectory, DeltaManifest);
		StreamWriter Writer = System.IO.File.CreateText(ManifestFile);
		foreach (string Line in DeltaFiles)
		{
			Writer.WriteLine(Line);
		}
		Writer.Close();
	}

	#endregion

	#region Stage Command

	//@todo move this
	public static List<DeploymentContext> CreateDeploymentContext(ProjectParams Params, bool InDedicatedServer, bool DoCleanStage = false)
	{
		ParamList<string> ListToProcess = InDedicatedServer && (Params.Cook || Params.CookOnTheFly) ? Params.ServerCookedTargets : Params.ClientCookedTargets;
		var ConfigsToProcess = InDedicatedServer && (Params.Cook || Params.CookOnTheFly) ? Params.ServerConfigsToBuild : Params.ClientConfigsToBuild;
		var CreateWebSocketsServer = Params.ServerTargetPlatforms.Count() > 0 && Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.HTML5));

		List<TargetPlatformDescriptor> PlatformsToStage = Params.ClientTargetPlatforms;
		if (InDedicatedServer && (Params.Cook || Params.CookOnTheFly))
		{
			PlatformsToStage = Params.ServerTargetPlatforms;
        }

 		List<DeploymentContext> DeploymentContexts = new List<DeploymentContext>();
		foreach (var StagePlatform in PlatformsToStage)
		{
            // Get the platform to get cooked data from, may differ from the stage platform
            TargetPlatformDescriptor CookedDataPlatform = Params.GetCookedDataPlatformForClientTarget(StagePlatform);

			if (InDedicatedServer && (Params.Cook || Params.CookOnTheFly))
			{
				CookedDataPlatform = Params.GetCookedDataPlatformForServerTarget(StagePlatform);
			}

			List<string> ExecutablesToStage = new List<string>();

			string PlatformName = StagePlatform.ToString();
			string StageArchitecture = !String.IsNullOrEmpty(Params.SpecifiedArchitecture) ? Params.SpecifiedArchitecture : "";
			foreach (var Target in ListToProcess)
			{
				foreach (var Config in ConfigsToProcess)
				{
					string Exe = Target;
					if (Config != UnrealTargetConfiguration.Development)
					{
						Exe = Target + "-" + PlatformName + "-" + Config.ToString() + StageArchitecture;
					}
					ExecutablesToStage.Add(Exe);
				}
            }

			string StageDirectory = ((ShouldCreatePak(Params) || (Params.Stage)) || !String.IsNullOrEmpty(Params.StageDirectoryParam)) ? Params.BaseStageDirectory : "";
            string ArchiveDirectory = (Params.Archive || !String.IsNullOrEmpty(Params.ArchiveDirectoryParam)) ? Params.BaseArchiveDirectory : "";
			string EngineDir = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine");

			List<StageTarget> TargetsToStage = new List<StageTarget>();
			foreach(string Target in ListToProcess)
			{
				foreach(UnrealTargetConfiguration Config in ConfigsToProcess)
				{
					string ReceiptBaseDir = Params.IsCodeBasedProject? Path.GetDirectoryName(Params.RawProjectPath.FullName) : EngineDir;

					Platform PlatformInstance = Platform.Platforms[StagePlatform];
					UnrealTargetPlatform[] SubPlatformsToStage = PlatformInstance.GetStagePlatforms();

					// if we are attempting to gathering multiple platforms, the files aren't required
					bool bRequireStagedFilesToExist = SubPlatformsToStage.Length == 1 && PlatformsToStage.Count == 1;

					foreach (UnrealTargetPlatform ReceiptPlatform in SubPlatformsToStage)
					{
                        string Architecture = Params.SpecifiedArchitecture;
                        if (string.IsNullOrEmpty(Architecture))
                        {
                            Architecture = "";
                            var BuildPlatform = UEBuildPlatform.GetBuildPlatform(ReceiptPlatform, true);
                            if (BuildPlatform != null)
                            {
                                Architecture = BuildPlatform.CreateContext(Params.RawProjectPath).GetActiveArchitecture();
                            }
                        }
						string ReceiptFileName = TargetReceipt.GetDefaultPath(ReceiptBaseDir, Target, ReceiptPlatform, Config, Architecture);
						if(!File.Exists(ReceiptFileName))
						{
							if (bRequireStagedFilesToExist)
							{
								// if we aren't collecting multiple platforms, then it is expected to exist
                                throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Missing receipt '{0}'. Check that this target has been built.", Path.GetFileName(ReceiptFileName));
							}
							else
							{
								// if it's multiple platforms, then allow missing receipts
								continue;
							}

						}

						// Read the receipt for this target
						TargetReceipt Receipt;
						if(!TargetReceipt.TryRead(ReceiptFileName, out Receipt))
						{
							throw new AutomationException("Missing or invalid target receipt ({0})", ReceiptFileName);
						}

						// Convert the paths to absolute
						Receipt.ExpandPathVariables(new DirectoryReference(EngineDir), Params.RawProjectPath.Directory);
						TargetsToStage.Add(new StageTarget{ Receipt = Receipt, RequireFilesExist = bRequireStagedFilesToExist });
					}
				}
			}

			//@todo should pull StageExecutables from somewhere else if not cooked
            var SC = new DeploymentContext(Params.RawProjectPath, CmdEnv.LocalRoot,
                StageDirectory,
                ArchiveDirectory,
				Platform.Platforms[CookedDataPlatform],
                Platform.Platforms[StagePlatform],
				ConfigsToProcess,
				TargetsToStage,
				ExecutablesToStage,
				InDedicatedServer,
				Params.Cook || Params.CookOnTheFly,
				Params.CrashReporter,
				Params.Stage,
				Params.CookOnTheFly,
				Params.Archive,
				Params.IsProgramTarget,
				Params.Client,
				bInUseWebsocketNetDriver: CreateWebSocketsServer
				);
			LogDeploymentContext(SC);

			// If we're a derived platform make sure we're at the end, otherwise make sure we're at the front

			if (!CookedDataPlatform.Equals(StagePlatform))
			{
				DeploymentContexts.Add(SC);
			}
			else
			{
				DeploymentContexts.Insert(0, SC);
			}
		}

		return DeploymentContexts;
	}

	public static void CopyBuildToStagingDirectory(ProjectParams Params)
	{
		if (ShouldCreatePak(Params) || (Params.Stage && !Params.SkipStage))
		{
			Params.ValidateAndLog();

			Log("********** STAGE COMMAND STARTED **********");

			if (!Params.NoClient)
			{
				var DeployContextList = CreateDeploymentContext(Params, false, true);

				// clean the staging directories first
				foreach (var SC in DeployContextList)
				{
					// write out the commandline file now so it can go into the manifest
					WriteStageCommandline(Params, SC);
					CreateStagingManifest(Params, SC);
					CleanStagingDirectory(Params, SC);
				}
				foreach (var SC in DeployContextList)
				{
					ApplyStagingManifest(Params, SC);

					if (Params.Deploy)
					{
						List<string> UFSManifests;
						List<string> NonUFSManifests;

						// get the staged file data
						Dictionary<string, string> StagedUFSFiles = ReadStagedManifest(Params, SC, SC.UFSDeployedManifestFileName);
						Dictionary<string, string> StagedNonUFSFiles = ReadStagedManifest(Params, SC, SC.NonUFSDeployedManifestFileName);

                        foreach (var DeviceName in Params.DeviceNames)
                        {
                            string UniqueName = "";
                            if (SC.StageTargetPlatform.SupportsMultiDeviceDeploy)
                            {
                                UniqueName = DeviceName;
                            }

                            // get the deployed file data
                            Dictionary<string, string> DeployedUFSFiles = new Dictionary<string, string>();
                            Dictionary<string, string> DeployedNonUFSFiles = new Dictionary<string, string>();

                            if (SC.StageTargetPlatform.RetrieveDeployedManifests(Params, SC, DeviceName, out UFSManifests, out NonUFSManifests))
                            {
                                DeployedUFSFiles = ReadDeployedManifest(Params, SC, UFSManifests);
                                DeployedNonUFSFiles = ReadDeployedManifest(Params, SC, NonUFSManifests);
                            }
                            
                            WriteObsoleteManifest(Params, SC, DeployedUFSFiles, StagedUFSFiles, DeploymentContext.UFSDeployObsoleteFileName + UniqueName);
                            WriteObsoleteManifest(Params, SC, DeployedNonUFSFiles, StagedNonUFSFiles, DeploymentContext.NonUFSDeployObsoleteFileName + UniqueName);

                            if (Params.IterativeDeploy)
                            {
                                // write out the delta file data
                                WriteDeltaManifest(Params, SC, DeployedUFSFiles, StagedUFSFiles, DeploymentContext.UFSDeployDeltaFileName + UniqueName);
                                WriteDeltaManifest(Params, SC, DeployedNonUFSFiles, StagedNonUFSFiles, DeploymentContext.NonUFSDeployDeltaFileName + UniqueName);
                            }
                        }
					}

					if (Params.bCodeSign)
					{
						SC.StageTargetPlatform.SignExecutables(SC, Params);
					}
				}
			}

			if (Params.DedicatedServer)
			{
				var DeployContextList = CreateDeploymentContext(Params, true, true);
				foreach (var SC in DeployContextList)
				{
					CreateStagingManifest(Params, SC);
					CleanStagingDirectory(Params, SC);
				}

				foreach (var SC in DeployContextList)
				{
					ApplyStagingManifest(Params, SC);
				}
			}
			Log("********** STAGE COMMAND COMPLETED **********");
		}
	}

	#endregion
}
