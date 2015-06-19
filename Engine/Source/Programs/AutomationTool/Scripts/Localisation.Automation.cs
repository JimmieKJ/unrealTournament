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
    private void ExportProjectToDirectory(OneSkyService oneSkyService, ProjectGroup projectGroup, string ProjectName)
    {
        var project = GetProject(oneSkyService, projectGroup.Name, ProjectName);
        var file = project.UploadedFiles.FirstOrDefault(f => f.Filename == (ProjectName + ".po"));

        //Export
        if (file != null)
        {
            ExportFileToDirectory(file, new DirectoryInfo(CmdEnv.LocalRoot + "/Engine/Content/Localization/" + ProjectName), project.EnabledCultures);
        }
    }

    private int OneSkyDownloadedPOChangeList = 0;

	public override void ExecuteBuild()
	{
		var EditorExe = CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/Win64/UE4Editor-Cmd.exe");

		if (P4Enabled)
		{
			Log("Sync necessary content to head revision");
			P4.Sync(P4Env.BuildRootP4 + "/Engine/Config/...");
			P4.Sync(P4Env.BuildRootP4 + "/Engine/Content/...");
			P4.Sync(P4Env.BuildRootP4 + "/Engine/Source/...");
			Log("Localize from label {0}", P4Env.LabelToSync);
		}

        OneSkyConfigData OneSkyConfig = OneSkyConfigHelper.Find("OneSkyConfig_EpicGames");
        var oneSkyService = new OneSkyService(OneSkyConfig.ApiKey, OneSkyConfig.ApiSecret);
        var projectGroup = GetProjectGroup(oneSkyService, "Unreal Engine");

        // Create changelist for backed up POs from OneSky.
        if (P4Enabled)
        {
            OneSkyDownloadedPOChangeList = P4.CreateChange(P4Env.Client, "OneSky downloaded PO backup.");
        }

        // Export all text from OneSky
        ExportProjectToDirectory(oneSkyService, projectGroup, "Engine");
        ExportProjectToDirectory(oneSkyService, projectGroup, "Editor");
        ExportProjectToDirectory(oneSkyService, projectGroup, "EditorTutorials");
        ExportProjectToDirectory(oneSkyService, projectGroup, "PropertyNames");
        ExportProjectToDirectory(oneSkyService, projectGroup, "ToolTips");

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
		if (P4Enabled) { CommandletSCCArguments += (string.IsNullOrEmpty(CommandletSCCArguments) ? "" : " ") + "-EnableSCC"; }
		if (!AllowSubmit) { CommandletSCCArguments += (string.IsNullOrEmpty(CommandletSCCArguments) ? "" : " ") + "-DisableSCCSubmit"; }

		// Setup commandlet arguments with configurations.
		var CommandletArgumentSets = new string[] 
			{
				String.Format("-config={0}", @"./Config/Localization/Engine.ini") + (string.IsNullOrEmpty(CommandletSCCArguments) ? "" : " " + CommandletSCCArguments),
				String.Format("-config={0}", @"./Config/Localization/Editor.ini") + (string.IsNullOrEmpty(CommandletSCCArguments) ? "" : " " + CommandletSCCArguments),
				String.Format("-config={0}", @"./Config/Localization/EditorTutorials.ini") + (string.IsNullOrEmpty(CommandletSCCArguments) ? "" : " " + CommandletSCCArguments),
				String.Format("-config={0}", @"./Config/Localization/PropertyNames.ini") + (string.IsNullOrEmpty(CommandletSCCArguments) ? "" : " " + CommandletSCCArguments),
				String.Format("-config={0}", @"./Config/Localization/ToolTips.ini") + (string.IsNullOrEmpty(CommandletSCCArguments) ? "" : " " + CommandletSCCArguments),
				String.Format("-config={0}", @"./Config/Localization/WordCount.ini"),
			};

		// Execute commandlet for each set of arguments.
		foreach (var CommandletArguments in CommandletArgumentSets)
		{
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
        UploadDirectoryToProject(GetProject(oneSkyService, "Unreal Engine", "Engine"), new DirectoryInfo(CmdEnv.LocalRoot + "/Engine/Content/Localization/Engine"), "*.po");
        UploadDirectoryToProject(GetProject(oneSkyService, "Unreal Engine", "Editor"), new DirectoryInfo(CmdEnv.LocalRoot + "/Engine/Content/Localization/Editor"), "*.po");
        UploadDirectoryToProject(GetProject(oneSkyService, "Unreal Engine", "EditorTutorials"), new DirectoryInfo(CmdEnv.LocalRoot + "/Engine/Content/Localization/EditorTutorials"), "*.po");
        UploadDirectoryToProject(GetProject(oneSkyService, "Unreal Engine", "PropertyNames"), new DirectoryInfo(CmdEnv.LocalRoot + "/Engine/Content/Localization/PropertyNames"), "*.po");
        UploadDirectoryToProject(GetProject(oneSkyService, "Unreal Engine", "ToolTips"), new DirectoryInfo(CmdEnv.LocalRoot + "/Engine/Content/Localization/ToolTips"), "*.po");

		// Localisation statistics estimator.
		if (P4Enabled)
		{
			// Available only for P4
			var EstimatorExePath = CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/TranslatedWordsCountEstimator.exe");
			var StatisticsFilePath = @"\\epicgames.net\root\UE3\Localization\WordCounts\udn.csv";

			var Arguments = string.Format(
				"{0} {1} {2} {3} {4}",
				StatisticsFilePath,
				P4Env.P4Port,
				P4Env.User,
				P4Env.Client,
				Environment.GetEnvironmentVariable("P4PASSWD"));

			var RunResult = Run(EstimatorExePath, Arguments);

			if (RunResult.ExitCode != 0)
			{
				throw new AutomationException("Error while executing TranslatedWordsCountEstimator with arguments '{0}'", Arguments);
			}
		}
	}

    private static ProjectGroup GetProjectGroup(OneSkyService oneSkyService, string GroupName)
    {
        var launcherGroup = oneSkyService.ProjectGroups.FirstOrDefault(g => g.Name == GroupName);

        if (launcherGroup == null)
        {
            launcherGroup = new ProjectGroup(GroupName, "en");
            oneSkyService.ProjectGroups.Add(launcherGroup);
        }

        return launcherGroup;
    }

    private static OneSky.Project GetProject(OneSkyService oneSkyService, string GroupName, string ProjectName, string ProjectDescription = "")
    {
        var launcherGroup = GetProjectGroup(oneSkyService, GroupName);

        OneSky.Project appProject = launcherGroup.Projects.FirstOrDefault(p => p.Name == ProjectName);

        if (appProject == null)
        {
            ProjectType projectType = oneSkyService.ProjectTypes.First(pt => pt.Code == "website");

            appProject = new OneSky.Project(ProjectName, ProjectDescription, projectType);
            launcherGroup.Projects.Add(appProject);
        }

        return appProject;
    }

    private void ExportFileToDirectory(UploadedFile file, DirectoryInfo destination, IEnumerable<string> cultures)
    {
        foreach (var culture in cultures)
        {
            var cultureDirectory = new DirectoryInfo(Path.Combine(destination.FullName, culture));
            if (!cultureDirectory.Exists)
            {
                cultureDirectory.Create();
            }

            using (var memoryStream = new MemoryStream())
            {
                var exportFile = new FileInfo(Path.Combine(cultureDirectory.FullName, file.Filename));

                var exportTranslationState = file.ExportTranslation(culture, memoryStream).Result;
                if (exportTranslationState == UploadedFile.ExportTranslationState.Success)
                {
                    memoryStream.Position = 0;
                    using (Stream fileStream = File.OpenWrite(exportFile.FullName))
                    {
                        memoryStream.CopyTo(fileStream);
                        Console.WriteLine("[SUCCESS] Exporting: " + exportFile.FullName + " Locale: " + culture);
                    }
                    FileInfo exportFileCopy = new FileInfo(Path.Combine(exportFile.DirectoryName, Path.GetFileNameWithoutExtension(exportFile.Name) + "_FromOneSky" + exportFile.Extension));
                    // Add/check out backed up POs from OneSky.
                    if (P4Enabled)
                    {
                        UE4Build.AddBuildProductsToChangelist(OneSkyDownloadedPOChangeList, new List<string>() {exportFileCopy.FullName} );
                    }
                    File.Copy(exportFile.FullName, exportFileCopy.FullName, true);
                }
                else if (exportTranslationState == UploadedFile.ExportTranslationState.NoContent)
                {
                    Console.WriteLine("[WARNING] Exporting: " + exportFile.FullName + " Locale: " + culture + " has no translations!");
                }
                else
                {
                    Console.WriteLine("[FAILED] Exporting: " + exportFile.FullName + " Locale: " + culture);
                }
            }
        }
    }

    static void UploadFilesToProjectForLocale(OneSky.Project project, string[] files, string localeName)
    {
        foreach (var currentFile in files)
        {
            using (var fileStream = File.OpenRead(currentFile))
            {
                // Read the BOM
                var bom = new byte[3];
                fileStream.Read(bom, 0, 3);

                //We want to ignore the utf8 BOM
                if (bom[0] != 0xef || bom[1] != 0xbb || bom[2] != 0xbf)
                {
                    fileStream.Position = 0;
                }

                Console.WriteLine("Uploading: " + currentFile + " Locale: " + localeName);
                var uploadedFile = project.Upload(Path.GetFileName(currentFile), fileStream, localeName).Result;
                
                if (uploadedFile == null)
                {
                    Console.WriteLine("[FAILED] Uploading: " + currentFile + " Locale: " + localeName);
                }
                else
                {
                    Console.WriteLine("[SUCCESS] Uploading: " + currentFile + " Locale: " + localeName);                    
                }
            }
        }
    }

    static void UploadDirectoryToProject(OneSky.Project project, DirectoryInfo directory, string fileExtension)
    {
        DirectoryInfo[] cultureDirectories = directory.GetDirectories("*", SearchOption.TopDirectoryOnly);

        // Upload, synchronously, the base culture POs.
        DirectoryInfo nativeCultureDirectory = Array.Find(cultureDirectories, d => { return string.Equals(d.Name, project.BaseCultureName, StringComparison.InvariantCultureIgnoreCase); });
        if (nativeCultureDirectory != null)
        {
            string[] files = Directory.GetFiles(nativeCultureDirectory.FullName, fileExtension, SearchOption.AllDirectories);
            UploadFilesToProjectForLocale(project, files, nativeCultureDirectory.Name);
        }

        // Upload all other POs asynchronously.
        foreach (var foreignCultureDirectory in Array.FindAll(cultureDirectories, d => { return d != nativeCultureDirectory; }))
        {
            string[] files = Directory.GetFiles(foreignCultureDirectory.FullName, fileExtension, SearchOption.AllDirectories);
            UploadFilesToProjectForLocale(project, files, foreignCultureDirectory.Name);
        }
    }
}

