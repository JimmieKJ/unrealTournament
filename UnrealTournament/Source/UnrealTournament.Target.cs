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
	}
    
    public override void SetupGlobalEnvironment(
        TargetInfo Target,
        ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
        ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
        )
    {
        // Turn on shipping logging, this will only apply to monolithic builds
        UEBuildConfiguration.bUseLoggingInShipping = true;

        UEBuildConfiguration.bBuildWithEditorOnlyData = true;
    }

    public override List<GUBPFormalBuild> GUBP_GetConfigsForFormalBuilds_MonolithicOnly(UnrealTargetPlatform HostPlatform)
    {
        return new List<GUBPFormalBuild> 
        { 
            new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development) 
        };
    }

}
