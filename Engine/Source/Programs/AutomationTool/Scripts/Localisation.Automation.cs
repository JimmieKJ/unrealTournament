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

class Localise : BuildCommand
{
	struct ProjectInfo
	{
		/** The name of this project */
		public string ProjectName;

		/** Path to this projects localization config file (relative to the root working directory for the commandlet) */
		public string LocalizationConfigFile;

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
	};

    private int OneSkyDownloadedPOChangeList = 0;

	public override void ExecuteBuild()
	{
		var EditorExe = CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/Win64/UE4Editor-Cmd.exe");

		// todo: All of this should be passed in via the command line to allow this script to be generic
		var UEProjectDirectory = "Engine";
		var OneSkyConfigName = "OneSkyConfig_EpicGames";
		var OneSkyProjectGroupName = "Unreal Engine";
		var OneSkyProjectNames = new string[] {
			"Engine",
			"Editor",
			"EditorTutorials",
			"PropertyNames",
			"ToolTips",
			"Category",
			"Keywords",
		};

		var RootWorkingDirectory = CombinePaths(CmdEnv.LocalRoot, UEProjectDirectory);

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
			ExportOneSkyProjectToDirectory(RootWorkingDirectory, OneSkyService, OneSkyProjectGroup, ProjectInfo);
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

		// Execute commandlet for each project.
		foreach (var ProjectInfo in ProjectInfos)
		{
			var CommandletArguments = String.Format("-config={0}", ProjectInfo.LocalizationConfigFile) + (String.IsNullOrEmpty(CommandletSCCArguments) ? "" : " " + CommandletSCCArguments);

			Log("Localization for {0} {1}", EditorArguments, CommandletArguments);

			Log("Running UE4Editor to generate localization data");

			string Arguments = String.Format("-run=GatherText {0} {1}", EditorArguments, CommandletArguments);
			var RunResult = Run(EditorExe, Arguments);

			if (RunResult.ExitCode != 0)
			{
				throw new AutomationException("Error while executing localization commandlet '{0}'", Arguments);
			}
		}

        // Upload all text to OneSky
		foreach (var ProjectInfo in ProjectInfos)
		{
			UploadProjectToOneSky(RootWorkingDirectory, OneSkyService, OneSkyProjectGroup, ProjectInfo);
		}
	}

	private ProjectInfo GenerateProjectInfo(string RootWorkingDirectory, string ProjectName)
	{
		var ProjectInfo = new ProjectInfo();

		ProjectInfo.ProjectName = ProjectName;
		ProjectInfo.LocalizationConfigFile = String.Format(@"Config/Localization/{0}.ini", ProjectName);

		var LocalizationConfig = new ConfigCacheIni(new FileReference(CombinePaths(RootWorkingDirectory, ProjectInfo.LocalizationConfigFile)));

		if (!LocalizationConfig.GetString("CommonSettings", "DestinationPath", out ProjectInfo.DestinationPath))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'DestinationPath', File: '{0}'", ProjectInfo.LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetString("CommonSettings", "ManifestName", out ProjectInfo.ManifestName))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'ManifestName', File: '{0}'", ProjectInfo.LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetString("CommonSettings", "ArchiveName", out ProjectInfo.ArchiveName))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'ArchiveName', File: '{0}'", ProjectInfo.LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetString("CommonSettings", "PortableObjectName", out ProjectInfo.PortableObjectName))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'PortableObjectName', File: '{0}'", ProjectInfo.LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetString("CommonSettings", "NativeCulture", out ProjectInfo.NativeCulture))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'NativeCulture', File: '{0}'", ProjectInfo.LocalizationConfigFile);
		}

		if (!LocalizationConfig.GetArray("CommonSettings", "CulturesToGenerate", out ProjectInfo.CulturesToGenerate))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'CulturesToGenerate', File: '{0}'", ProjectInfo.LocalizationConfigFile);
		}

		return ProjectInfo;
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

	private void ExportOneSkyProjectToDirectory(string RootWorkingDirectory, OneSkyService OneSkyService, ProjectGroup OneSkyProjectGroup, ProjectInfo ProjectInfo)
	{
		var OneSkyProject = GetOneSkyProject(OneSkyService, OneSkyProjectGroup.Name, ProjectInfo.ProjectName);
		var OneSkyFile = OneSkyProject.UploadedFiles.FirstOrDefault(f => f.Filename == ProjectInfo.PortableObjectName);

		//Export
		if (OneSkyFile != null)
		{
			var CulturesToExport = new List<string>();
			foreach (var OneSkyCulture in OneSkyProject.EnabledCultures)
			{
				// Only export the OneSky cultures that we care about for this project
				if (ProjectInfo.CulturesToGenerate.Contains(OneSkyCulture))
				{
					CulturesToExport.Add(OneSkyCulture);
				}
			}

			ExportOneSkyFileToDirectory(OneSkyFile, new DirectoryInfo(CombinePaths(RootWorkingDirectory, ProjectInfo.DestinationPath)), CulturesToExport);
		}
	}

	private void ExportOneSkyFileToDirectory(UploadedFile OneSkyFile, DirectoryInfo DestinationDirectory, IEnumerable<string> Cultures)
    {
		foreach (var Culture in Cultures)
        {
			var CultureDirectory = new DirectoryInfo(Path.Combine(DestinationDirectory.FullName, Culture));
			if (!CultureDirectory.Exists)
            {
				CultureDirectory.Create();
            }

            using (var MemoryStream = new MemoryStream())
            {
				var ExportFile = new FileInfo(Path.Combine(CultureDirectory.FullName, OneSkyFile.Filename));

				var ExportTranslationState = OneSkyFile.ExportTranslation(Culture, MemoryStream).Result;
				if (ExportTranslationState == UploadedFile.ExportTranslationState.Success)
                {
					MemoryStream.Position = 0;
					using (Stream FileStream = File.OpenWrite(ExportFile.FullName))
                    {
						MemoryStream.CopyTo(FileStream);
						Console.WriteLine("[SUCCESS] Exporting: '{0}' ({1})", ExportFile.FullName, Culture);
                    }

					FileInfo ExportFileCopy = new FileInfo(Path.Combine(ExportFile.DirectoryName, String.Format("{0}_FromOneSky{1}", Path.GetFileNameWithoutExtension(ExportFile.Name), ExportFile.Extension)));
					File.Copy(ExportFile.FullName, ExportFileCopy.FullName, true);

					// Add/check out backed up POs from OneSky.
					if (P4Enabled)
					{
						UE4Build.AddBuildProductsToChangelist(OneSkyDownloadedPOChangeList, new List<string>() { ExportFileCopy.FullName });
					}
                }
				else if (ExportTranslationState == UploadedFile.ExportTranslationState.NoContent)
                {
					Console.WriteLine("[WARNING] Exporting: '{0}' ({1}) has no translations!", ExportFile.FullName, Culture);
                }
                else
                {
					Console.WriteLine("[FAILED] Exporting: '{0}' ({1})", ExportFile.FullName, Culture);
                }
            }
        }
    }

	private void UploadProjectToOneSky(string RootWorkingDirectory, OneSkyService OneSkyService, ProjectGroup OneSkyProjectGroup, ProjectInfo ProjectInfo)
	{
		var OneSkyProject = GetOneSkyProject(OneSkyService, OneSkyProjectGroup.Name, ProjectInfo.ProjectName);

		// Upload the .po file for the native culture first
		UploadFileToOneSky(OneSkyProject, new FileInfo(Path.Combine(RootWorkingDirectory, ProjectInfo.DestinationPath, ProjectInfo.NativeCulture, ProjectInfo.PortableObjectName)), ProjectInfo.NativeCulture);

		// Upload the remaining .po files for the other cultures
		foreach (var Culture in ProjectInfo.CulturesToGenerate)
		{
			// Skip native culture as we uploaded it above
			if (Culture != ProjectInfo.NativeCulture)
			{
				UploadFileToOneSky(OneSkyProject, new FileInfo(Path.Combine(RootWorkingDirectory, ProjectInfo.DestinationPath, Culture, ProjectInfo.PortableObjectName)), Culture);	
			}
		}
	}

	private void UploadFileToOneSky(OneSky.Project OneSkyProject, FileInfo FileToUpload, string Culture)
	{
		var FileNameToUpload = FileToUpload.FullName;
		using (var FileStream = File.OpenRead(FileNameToUpload))
		{
			// Read the BOM
			var UTF8BOM = new byte[3];
			FileStream.Read(UTF8BOM, 0, 3);

			// We want to ignore the utf8 BOM
			if (UTF8BOM[0] != 0xef || UTF8BOM[1] != 0xbb || UTF8BOM[2] != 0xbf)
			{
				FileStream.Position = 0;
			}

			Console.WriteLine("Uploading: '{0}' ({1})", FileNameToUpload, Culture);

			// todo: need to use the branch suffix here so that OneSky will merge them together
			var UploadedFile = OneSkyProject.Upload(Path.GetFileName(FileNameToUpload), FileStream, Culture).Result;

			if (UploadedFile == null)
			{
				Console.WriteLine("[FAILED] Uploading: '{0}' ({1})", FileNameToUpload, Culture);
			}
			else
			{
				Console.WriteLine("[SUCCESS] Uploading: '{0}' ({1})", FileNameToUpload, Culture);
			}
		}
	}
}
