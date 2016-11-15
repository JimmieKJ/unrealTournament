// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class UnrealCEFSubProcessTarget : TargetRules
{
	public UnrealCEFSubProcessTarget(TargetInfo Target)
	{
		Type = TargetType.Program;
	}

	//
	// TargetRules interface.
	//

	public override bool GetSupportedPlatforms(ref List<UnrealTargetPlatform> OutPlatforms)
	{
		OutPlatforms.Add(UnrealTargetPlatform.Win32);
		OutPlatforms.Add(UnrealTargetPlatform.Win64);
		OutPlatforms.Add(UnrealTargetPlatform.Mac);
		OutPlatforms.Add(UnrealTargetPlatform.Linux);
		return true;
	}

	public override bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
	{
		return true;
	}

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutBuildBinaryConfigurations.Add(
			new UEBuildBinaryConfiguration(InType: UEBuildBinaryType.Executable,
											InModuleNames: new List<string>() { "UnrealCEFSubProcess" })
			);
	}

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		// Turn off various third party features we don't need

		// Currently we force Lean and Mean mode
		UEBuildConfiguration.bCompileLeanAndMeanUE = true;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		UEBuildConfiguration.bCompileAgainstEngine = false;
		UEBuildConfiguration.bCompileAgainstCoreUObject = false;
		UEBuildConfiguration.bBuildWithEditorOnlyData = true;

		// Never use malloc profiling in CEFSubProcess.
		BuildConfiguration.bUseMallocProfiler = false;

		// Force all shader formats to be built and included.
		//UEBuildConfiguration.bForceBuildShaderFormats = true;

		// CEFSubProcess is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = false;

		// Disable logging, as the sub processes are spawned often and logging will just slow them down
		OutCPPEnvironmentConfiguration.Definitions.Add("ALLOW_LOG_FILE=0");

		// Epic Games Launcher needs to run on OS X 10.9, so CEFSubProcess needs this as well
		OutCPPEnvironmentConfiguration.bEnableOSX109Support = true;
	}

    public override bool GUBP_AlwaysBuildWithTools(UnrealTargetPlatform InHostPlatform, out bool bInternalToolOnly, out bool SeparateNode, out bool CrossCompile)
    {
        bInternalToolOnly = false;
        SeparateNode = false;
        CrossCompile = false;
        return true;
    }

    public override List<UnrealTargetPlatform> GUBP_ToolPlatforms(UnrealTargetPlatform InHostPlatform)
    {
		if(InHostPlatform == UnrealTargetPlatform.Win64)
		{
			return new List<UnrealTargetPlatform>{ UnrealTargetPlatform.Win32, UnrealTargetPlatform.Win64 };
		}
		else
		{
			return new List<UnrealTargetPlatform> { InHostPlatform };
		}
    }

    public override bool GUBP_NeedsPlatformSpecificDLLs()
    {
        return true;
    }
}
