// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealTournamentTarget : TargetRules
{
	public UnrealTournamentTarget(TargetInfo Target)
	{
        Type = TargetType.Game;

        if (!UnrealBuildTool.UnrealBuildTool.RunningRocket())
        {
            OutExtraModuleNames.Add("OnlineSubsystemMcp");
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
		OutExtraModuleNames.Add("UnrealTournament");
        if (UEBuildConfiguration.bBuildEditor)
        {
            OutExtraModuleNames.Add("UnrealTournamentEditor");
        }
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

    public override List<GUBPFormalBuild> GUBP_GetConfigsForFormalBuilds_MonolithicOnly(UnrealTargetPlatform HostPlatform)
    {
        return new List<GUBPFormalBuild> 
        { 
            new GUBPFormalBuild(UnrealTargetConfiguration.Development) 
        };
    }

    public override List<UnrealTargetPlatform> GUBP_GetPlatforms_MonolithicOnly(UnrealTargetPlatform HostPlatform)
    {
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            return new List<UnrealTargetPlatform> { UnrealTargetPlatform.Mac };
        }
        return new List<UnrealTargetPlatform> { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Linux };
    }
}
