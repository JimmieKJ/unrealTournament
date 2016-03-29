// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;

[Flags]
public enum ProjectBuildTargets
{
	None = 0,
	Editor = 1 << 0,
	ClientCooked = 1 << 1,
	ServerCooked = 1 << 2,
	Bootstrap = 1 << 3,
	CrashReporter = 1 << 4,
	Programs = 1 << 5,

	// All targets
	All = Editor | ClientCooked | ServerCooked | Bootstrap | CrashReporter | Programs,
}

/// <summary>
/// Helper command used for compiling.
/// </summary>
/// <remarks>
/// Command line params used by this command:
/// -cooked
/// -cookonthefly
/// -clean
/// -[platform]
/// </remarks>
public partial class Project : CommandUtils
{
	#region Build Command

    public static void Build(BuildCommand Command, ProjectParams Params, int WorkingCL = -1, ProjectBuildTargets TargetMask = ProjectBuildTargets.All)
	{
		Params.ValidateAndLog();

		if (!Params.Build)
		{
			return;
		}

		Log("********** BUILD COMMAND STARTED **********");

		var UE4Build = new UE4Build(Command);
		var Agenda = new UE4Build.BuildAgenda();
		var CrashReportPlatforms = new HashSet<UnrealTargetPlatform>();

		// Setup editor targets
		if (Params.HasEditorTargets && !Automation.IsEngineInstalled() && (TargetMask & ProjectBuildTargets.Editor) == ProjectBuildTargets.Editor)
		{
			// @todo Mac: proper platform detection
			UnrealTargetPlatform EditorPlatform = HostPlatform.Current.HostEditorPlatform;
			const UnrealTargetConfiguration EditorConfiguration = UnrealTargetConfiguration.Development;

			CrashReportPlatforms.Add(EditorPlatform);
            Agenda.AddTargets(Params.EditorTargets.ToArray(), EditorPlatform, EditorConfiguration, Params.CodeBasedUprojectPath);
			if (Params.EditorTargets.Contains("UnrealHeaderTool") == false)
			{
				Agenda.AddTargets(new string[] { "UnrealHeaderTool" }, EditorPlatform, EditorConfiguration);
			}
			if (Params.EditorTargets.Contains("ShaderCompileWorker") == false)
			{
				Agenda.AddTargets(new string[] { "ShaderCompileWorker" }, EditorPlatform, EditorConfiguration);
			}
			if (Params.Pak && Params.EditorTargets.Contains("UnrealPak") == false)
			{
				Agenda.AddTargets(new string[] { "UnrealPak" }, EditorPlatform, EditorConfiguration);
			}
			if (Params.FileServer && Params.EditorTargets.Contains("UnrealFileServer") == false)
			{
				Agenda.AddTargets(new string[] { "UnrealFileServer" }, EditorPlatform, EditorConfiguration);
			}
		}

        string ScriptPluginArgs = "";
        // if we're utilizing an auto-generated code plugin/module (a product of 
        // the cook process), make sure to compile it along with the targets here
        if (Params.RunAssetNativization)
        {
            // Add every plugin:
            foreach( var CodePlugin in  Params.BlueprintPluginPaths)
            {
                ScriptPluginArgs = "-PLUGIN \"" + CodePlugin + "\""; 
            }
        }

		// Setup cooked targets
		if (Params.HasClientCookedTargets && (TargetMask & ProjectBuildTargets.ClientCooked) == ProjectBuildTargets.ClientCooked)
		{
			foreach (var BuildConfig in Params.ClientConfigsToBuild)
			{
				foreach (var ClientPlatform in Params.ClientTargetPlatforms)
				{
					CrashReportPlatforms.Add(ClientPlatform);
					Agenda.AddTargets(Params.ClientCookedTargets.ToArray(), ClientPlatform, BuildConfig, Params.CodeBasedUprojectPath, InAddArgs: ScriptPluginArgs + " -remoteini=\"" + Params.RawProjectPath.Directory.FullName + "\"");
				}
			}
		}
		if (Params.HasServerCookedTargets && (TargetMask & ProjectBuildTargets.ServerCooked) == ProjectBuildTargets.ServerCooked)
		{
			foreach (var BuildConfig in Params.ServerConfigsToBuild)
			{
				foreach (var ServerPlatform in Params.ServerTargetPlatforms)
				{
					CrashReportPlatforms.Add(ServerPlatform);
					Agenda.AddTargets(Params.ServerCookedTargets.ToArray(), ServerPlatform, BuildConfig, Params.CodeBasedUprojectPath, InAddArgs: ScriptPluginArgs + " -remoteini=\"" + Params.RawProjectPath.Directory.FullName + "\"");
				}
			}
		}
		if (!Params.NoBootstrapExe && !Automation.IsEngineInstalled() && (TargetMask & ProjectBuildTargets.Bootstrap) == ProjectBuildTargets.Bootstrap)
		{
			UnrealBuildTool.UnrealTargetPlatform[] BootstrapPackagedGamePlatforms = { UnrealBuildTool.UnrealTargetPlatform.Win32, UnrealBuildTool.UnrealTargetPlatform.Win64 };
			foreach(UnrealBuildTool.UnrealTargetPlatform BootstrapPackagedGamePlatform in BootstrapPackagedGamePlatforms)
			{
				if(Params.ClientTargetPlatforms.Contains(BootstrapPackagedGamePlatform))
				{
					Agenda.AddTarget("BootstrapPackagedGame", BootstrapPackagedGamePlatform, UnrealBuildTool.UnrealTargetConfiguration.Shipping);
				}
			}
		}
		if (Params.CrashReporter && !Automation.IsEngineInstalled() && (TargetMask & ProjectBuildTargets.CrashReporter) == ProjectBuildTargets.CrashReporter)
		{
			foreach (var CrashReportPlatform in CrashReportPlatforms)
			{
				if (UnrealBuildTool.UnrealBuildTool.PlatformSupportsCrashReporter(CrashReportPlatform))
				{
					Agenda.AddTarget("CrashReportClient", CrashReportPlatform, UnrealTargetConfiguration.Shipping);
				}
			}
		}
		if (Params.HasProgramTargets && (TargetMask & ProjectBuildTargets.Programs) == ProjectBuildTargets.Programs)
		{
			foreach (var BuildConfig in Params.ClientConfigsToBuild)
			{
				foreach (var ClientPlatform in Params.ClientTargetPlatforms)
				{
					Agenda.AddTargets(Params.ProgramTargets.ToArray(), ClientPlatform, BuildConfig, Params.CodeBasedUprojectPath);
				}
			}
		}
		UE4Build.Build(Agenda, InDeleteBuildProducts: Params.Clean, InUpdateVersionFiles: WorkingCL > 0);

		if (WorkingCL > 0) // only move UAT files if we intend to check in some build products
		{
			UE4Build.AddUATFilesToBuildProducts();
		}
		UE4Build.CheckBuildProducts(UE4Build.BuildProductFiles);

		if (WorkingCL > 0)
		{
			// Sign everything we built
			CodeSign.SignMultipleIfEXEOrDLL(Command, UE4Build.BuildProductFiles);

			// Open files for add or edit
			UE4Build.AddBuildProductsToChangelist(WorkingCL, UE4Build.BuildProductFiles);
		}

		Log("********** BUILD COMMAND COMPLETED **********");
	}

	#endregion
}
