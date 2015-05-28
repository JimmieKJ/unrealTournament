// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class BootstrapPackagedGameTarget : TargetRules
{
	public BootstrapPackagedGameTarget(TargetInfo Target)
	{
		Type = TargetType.Program;

		bUseStaticCRT = true;

		if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PreferredSubPlatform = "WindowsXP";
		}
	}

	//
	// TargetRules interface.
	//

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutBuildBinaryConfigurations.Add(
			new UEBuildBinaryConfiguration( InType: UEBuildBinaryType.Executable, InModuleNames: new List<string>() { "BootstrapPackagedGame" })
			);
	}

	public override bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
	{
		return true;
	}

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		BuildConfiguration.bUseUnityBuild = false;
		BuildConfiguration.bUseSharedPCHs = false;
		BuildConfiguration.bUseMallocProfiler = false;

		// Disable all parts of the editor
		UEBuildConfiguration.bCompileLeanAndMeanUE = true;
		UEBuildConfiguration.bCompileICU = false;
		UEBuildConfiguration.bBuildEditor = false;
		UEBuildConfiguration.bBuildWithEditorOnlyData = false;
		UEBuildConfiguration.bCompileAgainstEngine = false;
		UEBuildConfiguration.bCompileAgainstCoreUObject = false;
	}
	
    public override bool GUBP_AlwaysBuildWithTools(UnrealTargetPlatform InHostPlatform, out bool bInternalToolOnly, out bool SeparateNode, out bool CrossCompile)
	{
		bInternalToolOnly = false;
		SeparateNode = false;
		CrossCompile = false;
		return (InHostPlatform == UnrealTargetPlatform.Win64);
	}

	public override List<UnrealTargetPlatform> GUBP_ToolPlatforms(UnrealTargetPlatform InHostPlatform)
	{
		if (InHostPlatform == UnrealTargetPlatform.Win64)
		{
			return new List<UnrealTargetPlatform> { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Win32 };
		}
		return base.GUBP_ToolPlatforms(InHostPlatform);
	}
	
	public override List<UnrealTargetConfiguration> GUBP_ToolConfigs(UnrealTargetPlatform InHostPlatform)
	{
		return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Shipping };
	}
}
