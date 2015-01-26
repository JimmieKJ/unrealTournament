// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealTournamentTarget : TargetRules
{
	public UnrealTournamentTarget(TargetInfo Target)
	{
        Type = TargetType.Game;
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
		OutExtraModuleNames.Add("UnrealTournament");
        if (UEBuildConfiguration.bBuildEditor)
        {
            OutExtraModuleNames.Add("UnrealTournamentEditor");
        }
		if (UEBuildConfiguration.bCompileMcpOSS == true)
		{
			OutExtraModuleNames.Add("OnlineSubsystemMcp");
        }
        OutExtraModuleNames.Add("OnlineSubsystemNull");
	}
    
    public override void SetupGlobalEnvironment(
        TargetInfo Target,
        ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
        ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
        )
    {
        // Turn on shipping logging, this will only apply to monolithic builds
        UEBuildConfiguration.bUseLoggingInShipping = true;
    }

    public override bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
    {
        if (UnrealBuildTool.UnrealBuildTool.RunningRocket() || InPlatform == UnrealTargetPlatform.Mac)
        {
            return base.ShouldCompileMonolithic(InPlatform, InConfiguration);
        }

        return false;
    }

    public override List<UnrealTargetPlatform> GUBP_GetPlatforms_MonolithicOnly(UnrealTargetPlatform HostPlatform)
    {
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            return new List<UnrealTargetPlatform> { UnrealTargetPlatform.Mac };
        }
        return new List<UnrealTargetPlatform> { UnrealTargetPlatform.Win32, UnrealTargetPlatform.Win64, UnrealTargetPlatform.Linux };
    }

    public override List<UnrealTargetConfiguration> GUBP_GetConfigs_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
    {
        if (Platform != UnrealTargetPlatform.Linux)
        {
            // ORDER HERE MATTERS, THE FIRST ENTRY IS PUT IN Manifest_NonUFSFiles.txt AND THE FOLLOWING ARE PUT IN Manifest_DebugFiles.txt
            return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Test };
        }

        // Linux has linker errors in Test right now
        return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Development };
    }
}
