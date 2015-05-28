// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UE4ServerTarget : TargetRules
{
    public UE4ServerTarget(TargetInfo Target)
	{
		Type = TargetType.Server;
        bOutputToEngineBinaries = true;
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
		base.SetupBinaries(Target, ref OutBuildBinaryConfigurations, ref OutExtraModuleNames);
		OutExtraModuleNames.Add("UE4Game");
	}

	public override void SetupGlobalEnvironment(
        TargetInfo Target,
        ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
        ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
        )
    {
    }

	public override bool ShouldUseSharedBuildEnvironment(TargetInfo Target)
	{
		return true;
	}

	public override bool GetSupportedPlatforms(ref List<UnrealTargetPlatform> OutPlatforms)
	{
		// It is valid for only server platforms
		return UnrealBuildTool.UnrealBuildTool.GetAllServerPlatforms(ref OutPlatforms, false);
	}

    public override List<UnrealTargetPlatform> GUBP_GetPlatforms_MonolithicOnly(UnrealTargetPlatform HostPlatform)
    {
		List<UnrealTargetPlatform> Platforms = null;

		switch(HostPlatform)
		{
			case UnrealTargetPlatform.Linux:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform };
				break;

			case UnrealTargetPlatform.Win64:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Win32, UnrealTargetPlatform.Linux };
				break;

			default:
				Platforms = new List<UnrealTargetPlatform>();
				break;
		}

		return Platforms;
    }

    public override List<UnrealTargetConfiguration> GUBP_GetConfigs_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
    {
        return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Development };
    }
}
