// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;

[Help("Builds a target")]
[Help("Project", "Specify the project with the targets to build.")]
[Help("Target", "Specify a list of target descriptor files for the targets to build, separated by '+' characters (eg. -Target=Game+Client+Editor)")]
[Help("TargetPlatforms", "Specify a list of target platforms to build, separated by '+' characters (eg. -TargetPlatforms=Win32+Win64+IOS). Default is Win64.")]
[Help("Config", "Specify a list of target build configurations to build against, separated by '+' characters (eg. -Config=Debug+Test). Default is Development")]
class BuildTarget : BuildCommand
{
	private ParamList<string> ParseParamList(string InArgument, string InDefault = null)
	{
		var ArgumentList = ParseParamValue(InArgument);
		if (ArgumentList != null)
		{
			return new ParamList<string>(ArgumentList.Split('+'));
		}
		else if (!String.IsNullOrEmpty(InDefault))
		{
			return new ParamList<string>(InDefault);
		}
		return null;
	}

	public override void ExecuteBuild()
	{
		// get the project
		var UProjectFileName = ParseParamValue("Project");
		if (UProjectFileName == null)
		{
			throw new AutomationException("Project was not specified via the -project argument.");
		}

		// Get the list of targets
		var TargetList = ParseParamList("Target");
		if (TargetList == null)
		{
			throw new AutomationException("Target was not specified via the -target argument.");
		}

		// get the list of platforms
		var PlatformList = ParseParamList("TargetPlatforms", "Win64");
		List<UnrealTargetPlatform> TargetPlatforms = new List<UnrealTargetPlatform>();
		foreach(string Platform in PlatformList)
		{
			TargetPlatforms.Add((UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), Platform, true));
		}

		// get the list configurations
		var ConfigList = ParseParamList("Config", "Development");
		List<UnrealTargetConfiguration> ConfigsToBuild = new List<UnrealTargetConfiguration>();
		foreach(string Config in ConfigList)
		{
			ConfigsToBuild.Add((UnrealTargetConfiguration)Enum.Parse(typeof(UnrealTargetConfiguration), Config, true));
		}

		// parse any extra parameters
		bool bClean = ParseParam("Clean");
		int WorkingCL = ParseParamInt("P4Change");

        FileReference UProjectFileReference = new FileReference( UProjectFileName);

		// add the targets to the agenda
		// verify the targets and add them to the agenda
		var Properties = ProjectUtils.GetProjectProperties(UProjectFileReference);
		UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();
		foreach (string Target in TargetList)
		{
			SingleTargetProperties TargetData;
			if (!Properties.Targets.TryGetValue((TargetRules.TargetType)Enum.Parse(typeof(TargetRules.TargetType), Target), out TargetData))
			{
				throw new AutomationException("Project does not support specified target: {0}", Target);
			}

			foreach (UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				if (TargetData.Rules.SupportsPlatform(TargetPlatform))
				{
                    List<UnrealTargetConfiguration> SupportedConfigurations = new List<UnrealTargetConfiguration>();
                    TargetData.Rules.GetSupportedConfigurations(ref SupportedConfigurations, true);

					foreach (UnrealTargetConfiguration TargetConfig in ConfigsToBuild)
					{
						if (SupportedConfigurations.Contains(TargetConfig))
						{
                            Agenda.AddTarget(TargetData.TargetName, TargetPlatform, TargetConfig, UProjectFileReference);
						}
						else
						{
							Log("{0} doesn't support the {1} configuration. It will not be built.", TargetData.TargetName, TargetConfig);
						}
					}
				}
				else
				{
					Log("{0} doesn't support the {1} platform. It will not be built.", TargetData.TargetName, TargetPlatform);
				}
			}
		}


		// build it
		UE4Build Build = new UE4Build(this);
		Build.Build(Agenda, InDeleteBuildProducts: bClean, InUpdateVersionFiles: WorkingCL > 0);

		if (WorkingCL > 0) // only move UAT files if we intend to check in some build products
		{
			Build.AddUATFilesToBuildProducts();
		}
		UE4Build.CheckBuildProducts(Build.BuildProductFiles);

		if (WorkingCL > 0)
		{
			// Sign everything we built
			CodeSign.SignMultipleIfEXEOrDLL(this, Build.BuildProductFiles);

			// Open files for add or edit
			UE4Build.AddBuildProductsToChangelist(WorkingCL, Build.BuildProductFiles);
		}

	}
	#region Build Command

/*	public static void Build(BuildCommand Command, ProjectParams Params, int WorkingCL = -1)
	{
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
		if (!Params.NoBootstrapExe && !Params.Rocket)
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
		if (Params.CrashReporter && !Params.Rocket)
		{
			foreach (var CrashReportPlatform in CrashReportPlatforms)
			{
				if (UnrealBuildTool.UnrealBuildTool.PlatformSupportsCrashReporter(CrashReportPlatform))
				{
					Agenda.AddTarget("CrashReportClient", CrashReportPlatform, UnrealTargetConfiguration.Shipping);
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
	}*/

	#endregion
}
