// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;

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

	public static void Build(BuildCommand Command, ProjectParams Params, int WorkingCL = -1)
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
		if (Params.HasEditorTargets && !Params.Rocket)
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

		// Setup cooked targets
		if (Params.HasClientCookedTargets)
		{
			foreach (var BuildConfig in Params.ClientConfigsToBuild)
			{
				foreach (var ClientPlatform in Params.ClientTargetPlatforms)
				{
					CrashReportPlatforms.Add(ClientPlatform);
					Agenda.AddTargets(Params.ClientCookedTargets.ToArray(), ClientPlatform, BuildConfig, Params.CodeBasedUprojectPath);
				}
			}
		}
		if (Params.HasServerCookedTargets)
		{
			foreach (var BuildConfig in Params.ServerConfigsToBuild)
			{
				foreach (var ServerPlatform in Params.ServerTargetPlatforms)
				{
					CrashReportPlatforms.Add(ServerPlatform);
					Agenda.AddTargets(Params.ServerCookedTargets.ToArray(), ServerPlatform, BuildConfig, Params.CodeBasedUprojectPath);
				}
			}
		}
		if (Params.CrashReporter && !Params.Rocket)
		{
			var CrashReportClientTarget = new[] { "CrashReportClient" };
			foreach (var CrashReportPlatform in CrashReportPlatforms)
			{
				if (UnrealBuildTool.UnrealBuildTool.PlatformSupportsCrashReporter(CrashReportPlatform))
				{
					Agenda.AddTargets(CrashReportClientTarget, CrashReportPlatform, UnrealTargetConfiguration.Development);
				}
			}
		}
		if (Params.HasProgramTargets && !Params.Rocket)
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
