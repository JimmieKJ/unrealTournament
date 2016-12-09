// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class BootstrapPackagedGameTarget : TargetRules
{
	public BootstrapPackagedGameTarget(TargetInfo Target)
	{
		Type = TargetType.Program;

		bUseStaticCRT = true;
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

	public override bool GetSupportedConfigurations(ref List<UnrealTargetConfiguration> OutConfigurations, bool bIncludeTestAndShippingConfigs)
	{
		OutConfigurations.Add(UnrealTargetConfiguration.Debug);
		OutConfigurations.Add(UnrealTargetConfiguration.Development);
		OutConfigurations.Add(UnrealTargetConfiguration.Shipping);
		return true;
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

		// Disable all parts of the editor.
		UEBuildConfiguration.bCompileLeanAndMeanUE = true;
		UEBuildConfiguration.bCompileICU = false;
		UEBuildConfiguration.bBuildEditor = false;
		UEBuildConfiguration.bBuildWithEditorOnlyData = false;
		UEBuildConfiguration.bCompileAgainstEngine = false;
		UEBuildConfiguration.bCompileAgainstCoreUObject = false;

		if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			UEBuildConfiguration.PreferredSubPlatform = "WindowsXP";
		}
	}
}
