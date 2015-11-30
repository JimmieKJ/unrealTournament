// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using OneSky;
using EpicGames.OneSkyLocalization.Config;

[Help("Updates the OneSky localization data using the arguments provided.")]
[Help("UEProjectDirectory", "Sub-path to the project we're gathering for (relative to CmdEnv.LocalRoot).")]
[Help("UEProjectName", "Optional name of the project we're gathering for (should match its .uproject file, eg QAGame).")]
[Help("OneSkyConfigName", "Name of the config data to use (see OneSkyConfigHelper).")]
[Help("OneSkyProjectGroupName", "Name of the project group in OneSky.")]
[Help("OneSkyProjectNames", "Comma separated list of the projects in OneSky.")]
[Help("OneSkyBranchSuffix", "Optional suffix to use when uploading the new data to OneSky.")]
class Localise : BuildCommand
{
	struct ProjectImportExportInfo
	{
		/** The destination path for the files containing the gathered data for this project (extracted from its config file - relative to the root working directory for the commandlet) */
		public string DestinationPath;

		/** The name to use for this projects manifest file (extracted from its config file) */
		public string ManifestName;

		/** The name to use for this projects archive file (extracted from its config file) */
		public string ArchiveName;

		/** The name to use for this projects portable object file (extracted from its config file) */
		public string PortableObjectName;

		/** The native culture for this project (extracted from its config file) */
		public string NativeCulture;

		/** The cultures to generate for this project (extracted from its config file) */
		public List<string> CulturesToGenerate;

		/** True if we should use a per-culture directly when importing/exporting */
		public bool bUseCultureDirectory;
	};

	struct ProjectInfo
	{
		/** The name of this project */
		public string ProjectName;

		/** Path to this projects localization config files (relative to the root working directory for the commandlet) - ordered so that iterating them runs in the correct order */
		public List<string> LocalizationConfigFiles;

		/** Config data used by the PO file import process */
		public ProjectImportExportInfo ImportInfo;

		/** Config data used by the PO file export process */
		public ProjectImportExportInfo ExportInfo;
	};

    private int OneSkyDownloadedPOChangeList = 0;

	public override void ExecuteBuild()
	{
		var EditorExe = CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/Win64/UE4Editor-Cmd.exe");

		// Parse out the required command line arguments
		var UEProjectDirectory = ParseParamValue("UEProjectDirectory");
		if (UEProjectDirectory == null)
		{
			throw new AutomationException("Missing required command line argument: 'UEProjectDirectory'");
		}

		var UEProjectName = ParseParamValue("UEProjectName");
		if (UEProjectName == null)
		{
			UEProjectName = "";
		}

		var OneSkyConfigName = ParseParamValue("OneSkyConfigName");
		if (OneSkyConfigName == null)
		{
			throw new AutomationException("Missing required command line argument: 'OneSkyConfigName'");
		}

		var OneSkyProjectGroupName = ParseParamValue("OneSkyProjectGroupName");
		if (OneSkyProjectGroupName == null)
		{
			throw new AutomationException("Missing required command line argument: 'OneSkyProjectGroupName'");
		}

		var OneSkyProjectNames = new List<string>();
		{
			var OneSkyProjectNamesStr = ParseParamValue("OneSkyProjectNames");
			if (OneSkyProjectNamesStr == null)
			{
				throw new AutomationException("Missing required command line argument: 'OneSkyProjectNames'");
			}
			foreach (var ProjectName in OneSkyProjectNamesStr.Split(','))
			{
				OneSkyProjectNames.Add(ProjectName.Trim());
			}
		}

		var OneSkyBranchSuffix = ParseParamValue("OneSkyBranchSuffix");

		var RootWorkingDirectory = CombinePaths(CmdEnv.LocalRoot, UEProjectDirectory);

		// Make sure the Localization configs and content is up-to-date to ensure we don't get errors later on
		if (P4Enabled)
		{
			Log("Sync necessary content to head revision");
			P4.Sync(P4Env.BuildRootP4 + "/" + UEProjectDirectory + "/Config/Localization/...");
			P4.Sync(P4Env.BuildRootP4 + "/" + UEProjectDirectory + "/Content/Localization/...");
		}

		// Generate the info we need to gather for each project
		var ProjectInfos = new List<ProjectInfo>();
		foreach (var ProjectName in OneSkyProjectNames)
		{
			ProjectInfos.Add(GenerateProjectInfo(RootWorkingDirectory, ProjectName));
		}

		OneSkyConfigData OneSkyConfig = OneSkyConfigHelper.Find(OneSkyConfigName);
		var OneSkyService = new OneSkyService(OneSkyConfig.ApiKey, OneSkyConfig.ApiSecret);
		var OneSkyProjectGroup = GetOneSkyProjectGroup(OneSkyService, OneSkyProjectGroupName);

		// Create changelist for backed up POs from OneSky.
		if (P4Enabled)
		{
			OneSkyDownloadedPOChangeList = P4.CreateChange(P4Env.Client, "OneSky downloaded PO backup.");
		}

		// Export all text from OneSky
		foreach (var ProjectInfo in ProjectInfos)
		{
			ExportOneSkyProjectToDirectory(RootWorkingDirectory, OneSkyService, OneSkyProjectGroup, OneSkyBranchSuffix, ProjectInfo);
		}

		// Submit changelist for backed up POs from OneSky.
		if (P4Enabled)
		{
			int SubmittedChangeList;
			P4.Submit(OneSkyDownloadedPOChangeList, out SubmittedChangeList);
		}

		// Setup editor arguments for SCC.
		string EditorArguments = String.Empty;
		if (P4Enabled)
		{
			EditorArguments = String.Format("-SCCProvider={0} -P4Port={1} -P4User={2} -P4Client={3} -P4Passwd={4}", "Perforce", P4Env.P4Port, P4Env.User, P4Env.Client, P4.GetAuthenticationToken());
		}
		else
		{
			EditorArguments = String.Format("-SCCProvider={0}", "None");
		}

		// Setup commandlet arguments for SCC.
		string CommandletSCCArguments = String.Empty;
		if (P4Enabled) { CommandletSCCArguments += (String.IsNullOrEmpty(CommandletSCCArguments) ? "" : " ") + "-EnableSCC"; }
		if (!AllowSubmit) { CommandletSCCArguments += (String.IsNullOrEmpty(CommandletSCCArguments) ? "" : " ") + "-DisableSCCSubmit"; }

		// Execute commandlet for each config in each project.
		foreach (var ProjectInfo in ProjectInfos)
		{
			foreach (var LocalizationConfigFile in ProjectInfo.LocalizationConfigFiles)
			{
				var CommandletArguments = String.Format("-config={0}", LocalizationConfigFile) + (String.IsNullOrEmpty(CommandletSCCArguments) ? "" : " " + CommandletSCCArguments);

				Log("Localization for {0} {1}", EditorArguments, CommandletArguments);

				Log("Running UE4Editor to generate localization data");

				string Arguments = String.Format("{0} -run=GatherText {1} {2}", UEProjectName, EditorArguments, CommandletArguments);
				var RunResult = Run(EditorExe, Arguments);

				if (RunResult.ExitCode != 0)
				{
					Console.WriteLine("[ERROR] Error while executing localization commandlet '{0}'", Arguments);
				}
			}
		}

		// Upload all text to OneSky
		foreach (var ProjectInfo in ProjectInfos)
		{
			UploadProjectToOneSky(RootWorkingDirectory, OneSkyService, OneSkyProjectGroup, OneSkyBranchSuffix, ProjectInfo);
		}
	}

	private ProjectInfo GenerateProjectInfo(string RootWorkingDirectory, string ProjectName)
	{
		var ProjectInfo = new ProjectInfo();

		ProjectInfo.ProjectName = ProjectName;
		ProjectInfo.LocalizationConfigFiles = new List<string>();

		// Projects generated by the localization dashboard will use multiple config files that must be run in a specific order
		// Older projects (such as the Engine) would use a single config file containing all the steps
		// Work out which kind of project we're dealing with...
		var MonolithicConfigFile = String.Format(@"Config/Localization/{0}.ini", ProjectName);
		var IsMonolithicConfig = File.Exists(CombinePaths(RootWorkingDirectory, MonolithicConfigFile));
		if (IsMonolithicConfig)
		{
			ProjectInfo.LocalizationConfigFiles.Add(MonolithicConfigFile);

			ProjectInfo.ImportInfo = GenerateProjectImportExportInfo(RootWorkingDirectory, MonolithicConfigFile);
			ProjectInfo.ExportInfo = ProjectInfo.ImportInfo;
		}
		else
		{
			var FileSuffixes = new[] { 
				new { Suffix = "Gather", Required = true }, 
				new { Suffix = "Import", Required = true }, 
				new { Suffix = "Export", Required = true }, 
				new { Suffix = "Compile", Required = true }, 
				new { Suffix = "GenerateReports", Required = false } 
			};

			foreach (var FileSuffix in FileSuffixes)
			{
				var ModularConfigFile = String.Format(@"Config/Localization/{0}_{1}.ini", ProjectName, FileSuffix.Suffix);

				if (File.Exists(CombinePaths(RootWorkingDirectory, ModularConfigFile)))
				{
					ProjectInfo.LocalizationConfigFiles.Add(ModularConfigFile);

					if (FileSuffix.Suffix == "Import")
					{
						ProjectInfo.ImportInfo = GenerateProjectImportExportInfo(RootWorkingDirectory, ModularConfigFile);
					}
					else if (FileSuffix.Suffix == "Export")
					{
						ProjectInfo.ExportInfo = GenerateProjectImportExportInfo(RootWorkingDirectory, ModularConfigFile);
					}
				}
				else if (FileSuffix.Required)
				{
					throw new AutomationException("Failed to find a required config file! '{0}'", ModularConfigFile);
				}
			}
		}

		return ProjectInfo;
	}

	private ProjectImportExportInfo GenerateProjectImportExportInfo(string RootWorkingDirectory, string LocalizationConfigFile)
	{
		var ProjectImportExportInfo = new ProjectImportExportInfo();

		var LocalizationConfig = new ConfigCacheIni(new FileReference(CombinePaths(RootWorkingDirectory, LocalizationConfigFile)));

		if (!LocalizationConfig.GetString("CommonSettings", "DestinationPath", out ProjectImportExportInfo.DestinationPath))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'DestinationPath', File: '{0}'", LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetString("CommonSettings", "ManifestName", out ProjectImportExportInfo.ManifestName))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'ManifestName', File: '{0}'", LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetString("CommonSettings", "ArchiveName", out ProjectImportExportInfo.ArchiveName))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'ArchiveName', File: '{0}'", LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetString("CommonSettings", "PortableObjectName", out ProjectImportExportInfo.PortableObjectName))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'PortableObjectName', File: '{0}'", LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetString("CommonSettings", "NativeCulture", out ProjectImportExportInfo.NativeCulture))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'NativeCulture', File: '{0}'", LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetArray("CommonSettings", "CulturesToGenerate", out ProjectImportExportInfo.CulturesToGenerate))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'CulturesToGenerate', File: '{0}'", LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetBool("CommonSettings", "bUseCultureDirectory", out ProjectImportExportInfo.bUseCultureDirectory))
		{
			// bUseCultureDirectory is optional, default is true
			ProjectImportExportInfo.bUseCultureDirectory = true;
		}

		return ProjectImportExportInfo;
	}

	private static ProjectGroup GetOneSkyProjectGroup(OneSkyService OneSkyService, string ProjectGroupName)
    {
		var OneSkyProjectGroup = OneSkyService.ProjectGroups.FirstOrDefault(g => g.Name == ProjectGroupName);

		if (OneSkyProjectGroup == null)
        {
			OneSkyProjectGroup = new ProjectGroup(ProjectGroupName, "en");
			OneSkyService.ProjectGroups.Add(OneSkyProjectGroup);
        }

		return OneSkyProjectGroup;
    }

	private static OneSky.Project GetOneSkyProject(OneSkyService OneSkyService, string GroupName, string ProjectName, string ProjectDescription = "")
    {
		var OneSkyProjectGroup = GetOneSkyProjectGroup(OneSkyService, GroupName);

		OneSky.Project OneSkyProject = OneSkyProjectGroup.Projects.FirstOrDefault(p => p.Name == ProjectName);

		if (OneSkyProject == null)
        {
			ProjectType projectType = OneSkyService.ProjectTypes.First(pt => pt.Code == "website");

			OneSkyProject = new OneSky.Project(ProjectName, ProjectDescription, projectType);
			OneSkyProjectGroup.Projects.Add(OneSkyProject);
        }

		return OneSkyProject;
    }

	private void ExportOneSkyProjectToDirectory(string RootWorkingDirectory, OneSkyService OneSkyService, ProjectGroup OneSkyProjectGroup, string OneSkyBranchSuffix, ProjectInfo ProjectInfo)
	{
		var OneSkyFileName = GetOneSkyFilename(ProjectInfo.ImportInfo.PortableObjectName, OneSkyBranchSuffix);
		var OneSkyProject = GetOneSkyProject(OneSkyService, OneSkyProjectGroup.Name, ProjectInfo.ProjectName);
		var OneSkyFile = OneSkyProject.UploadedFiles.FirstOrDefault(f => f.Filename == OneSkyFileName);

		//Export
		if (OneSkyFile != null)
		{
			var CulturesToExport = new List<string>();
			foreach (var OneSkyCulture in OneSkyProject.EnabledCultures)
			{
				// Only export the OneSky cultures that we care about for this project
				if (ProjectInfo.ImportInfo.CulturesToGenerate.Contains(OneSkyCulture))
				{
					CulturesToExport.Add(OneSkyCulture);
				}
			}

			ExportOneSkyFileToDirectory(OneSkyFile, new DirectoryInfo(CombinePaths(RootWorkingDirectory, ProjectInfo.ImportInfo.DestinationPath)), ProjectInfo.ImportInfo.PortableObjectName, CulturesToExport, ProjectInfo.ImportInfo.bUseCultureDirectory);
		}
	}

	private void ExportOneSkyFileToDirectory(UploadedFile OneSkyFile, DirectoryInfo DestinationDirectory, string DestinationFilename, IEnumerable<string> Cultures, bool bUseCultureDirectory)
    {
		foreach (var Culture in Cultures)
        {
			var CultureDirectory = (bUseCultureDirectory) ? new DirectoryInfo(Path.Combine(DestinationDirectory.FullName, Culture)) : DestinationDirectory;
			if (!CultureDirectory.Exists)
            {
				CultureDirectory.Create();
            }

            using (var MemoryStream = new MemoryStream())
            {
				var ExportTranslationState = OneSkyFile.ExportTranslation(Culture, MemoryStream).Result;
				if (ExportTranslationState == UploadedFile.ExportTranslationState.Success)
				{
					var ExportFile = new FileInfo(Path.Combine(CultureDirectory.FullName, DestinationFilename));

					// Write out the updated PO file so that the gather commandlet will import the new data from it
					{
						var ExportFileWasReadOnly = false;
						if (ExportFile.Exists)
						{
							// We're going to clobber the existing PO file, so make sure it's writable (it may be read-only if in Perforce)
							ExportFileWasReadOnly = ExportFile.IsReadOnly;
							ExportFile.IsReadOnly = false;
						}

						MemoryStream.Position = 0;
						using (Stream FileStream = ExportFile.OpenWrite())
						{
							MemoryStream.CopyTo(FileStream);
							Console.WriteLine("[SUCCESS] Exporting: '{0}' as '{1}' ({2})", OneSkyFile.Filename, ExportFile.FullName, Culture);
						}

						if (ExportFileWasReadOnly)
						{
							ExportFile.IsReadOnly = true;
						}
					}

					// Also update the back-up copy so we can diff against what we got from OneSky, and what the gather commandlet produced
					{
						var ExportFileCopy = new FileInfo(Path.Combine(ExportFile.DirectoryName, String.Format("{0}_FromOneSky{1}", Path.GetFileNameWithoutExtension(ExportFile.Name), ExportFile.Extension)));

						var ExportFileCopyWasReadOnly = false;
						if (ExportFileCopy.Exists)
						{
							// We're going to clobber the existing PO file, so make sure it's writable (it may be read-only if in Perforce)
							ExportFileCopyWasReadOnly = ExportFileCopy.IsReadOnly;
							ExportFileCopy.IsReadOnly = false;
						}

						ExportFile.CopyTo(ExportFileCopy.FullName, true);

						if (ExportFileCopyWasReadOnly)
						{
							ExportFileCopy.IsReadOnly = true;
						}

						// Add/check out backed up POs from OneSky.
						if (P4Enabled)
						{
							UE4Build.AddBuildProductsToChangelist(OneSkyDownloadedPOChangeList, new List<string>() { ExportFileCopy.FullName });
						}
					}
				}
				else if (ExportTranslationState == UploadedFile.ExportTranslationState.NoContent)
                {
					Console.WriteLine("[WARNING] Exporting: '{0}' ({1}) has no translations!", OneSkyFile.Filename, Culture);
                }
                else
                {
					Console.WriteLine("[FAILED] Exporting: '{0}' ({1})", OneSkyFile.Filename, Culture);
                }
            }
        }
    }

	private void UploadProjectToOneSky(string RootWorkingDirectory, OneSkyService OneSkyService, ProjectGroup OneSkyProjectGroup, string OneSkyBranchSuffix, ProjectInfo ProjectInfo)
	{
		var OneSkyProject = GetOneSkyProject(OneSkyService, OneSkyProjectGroup.Name, ProjectInfo.ProjectName);

		Func<string, FileInfo> GetPathForCulture = (string Culture) =>
		{
			if (ProjectInfo.ExportInfo.bUseCultureDirectory)
			{
				return new FileInfo(Path.Combine(RootWorkingDirectory, ProjectInfo.ExportInfo.DestinationPath, Culture, ProjectInfo.ExportInfo.PortableObjectName));
			}
			else
			{
				return new FileInfo(Path.Combine(RootWorkingDirectory, ProjectInfo.ExportInfo.DestinationPath, ProjectInfo.ExportInfo.PortableObjectName));
			}
		};

		// Upload the .po file for the native culture first
		UploadFileToOneSky(OneSkyProject, OneSkyBranchSuffix, GetPathForCulture(ProjectInfo.ExportInfo.NativeCulture), ProjectInfo.ExportInfo.NativeCulture);

		// Upload the remaining .po files for the other cultures
		foreach (var Culture in ProjectInfo.ExportInfo.CulturesToGenerate)
		{
			// Skip native culture as we uploaded it above
			if (Culture != ProjectInfo.ExportInfo.NativeCulture)
			{
				UploadFileToOneSky(OneSkyProject, OneSkyBranchSuffix, GetPathForCulture(Culture), Culture);	
			}
		}
	}

	private void UploadFileToOneSky(OneSky.Project OneSkyProject, string OneSkyBranchSuffix, FileInfo FileToUpload, string Culture)
	{
		using (var FileStream = FileToUpload.OpenRead())
		{
			// Read the BOM
			var UTF8BOM = new byte[3];
			FileStream.Read(UTF8BOM, 0, 3);

			// We want to ignore the utf8 BOM
			if (UTF8BOM[0] != 0xef || UTF8BOM[1] != 0xbb || UTF8BOM[2] != 0xbf)
			{
				FileStream.Position = 0;
			}

			var OneSkyFileName = GetOneSkyFilename(Path.GetFileName(FileToUpload.FullName), OneSkyBranchSuffix);

			Console.WriteLine("Uploading: '{0}' as '{1}' ({2})", FileToUpload.FullName, OneSkyFileName, Culture);

			var UploadedFile = OneSkyProject.Upload(OneSkyFileName, FileStream, Culture).Result;

			if (UploadedFile == null)
			{
				Console.WriteLine("[FAILED] Uploading: '{0}' ({1})", FileToUpload.FullName, Culture);
			}
			else
			{
				Console.WriteLine("[SUCCESS] Uploading: '{0}' ({1})", FileToUpload.FullName, Culture);
			}
		}
	}

	private static string GetOneSkyFilename(string BaseFilename, string OneSkyBranchSuffix)
	{
		var OneSkyFileName = BaseFilename;
		if (!String.IsNullOrEmpty(OneSkyBranchSuffix))
		{
			// Apply the branch suffix. OneSky will take care of merging the files from different branches together.
			var OneSkyFileNameWithSuffix = Path.GetFileNameWithoutExtension(OneSkyFileName) + "_" + OneSkyBranchSuffix + Path.GetExtension(OneSkyFileName);
			OneSkyFileName = OneSkyFileNameWithSuffix;
		}
		return OneSkyFileName;
	}
}
