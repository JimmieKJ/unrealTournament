// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UE4ClientTarget : TargetRules
{

    public UE4ClientTarget(TargetInfo Target)
	{
		Type = TargetType.Client;
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

    public override List<UnrealTargetPlatform> GUBP_GetPlatforms_MonolithicOnly(UnrealTargetPlatform HostPlatform)
    {
		List<UnrealTargetPlatform> Platforms = null;

		switch(HostPlatform)
		{
			case UnrealTargetPlatform.Mac:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
				break;

			case UnrealTargetPlatform.Linux:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform };
				break;

			case UnrealTargetPlatform.Win64:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Win32, UnrealTargetPlatform.Linux /*, UnrealTargetPlatform.IOS, UnrealTargetPlatform.XboxOne, UnrealTargetPlatform.PS4 */};
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
