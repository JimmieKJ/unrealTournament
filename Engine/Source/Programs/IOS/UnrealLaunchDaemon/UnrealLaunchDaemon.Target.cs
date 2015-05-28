// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealLaunchDaemonTarget : TargetRules
{
	public UnrealLaunchDaemonTarget(TargetInfo Target)
	{
		Type = TargetType.Program;
		bUsesSlate = false;
		//PlatformType = TargetRules.TargetPlatformType.Mobile;
		//bRequiresUnrealHeaderGeneration = true;
		AdditionalPlugins.Add("UdpMessaging");
	}

	//
	// TargetRules interface.
	//

	public override bool GetSupportedPlatforms(ref List<UnrealTargetPlatform> OutPlatforms)
	{
		OutPlatforms.Add(UnrealTargetPlatform.IOS);
		return true;
	}

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutBuildBinaryConfigurations.Add(
			new UEBuildBinaryConfiguration(	InType: UEBuildBinaryType.Executable,
											InModuleNames: new List<string>() { "UnrealLaunchDaemon" })
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
		UEBuildConfiguration.bCompileLeanAndMeanUE = true;
		UEBuildConfiguration.bBuildEditor = false;
		UEBuildConfiguration.bCompileAgainstEngine = false;

		OutLinkEnvironmentConfiguration.bHasExports = false;
	}
}
