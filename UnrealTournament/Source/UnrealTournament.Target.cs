// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;
using System.IO;

public class UnrealTournamentTarget : TargetRules
{
    bool IsLicenseeBuild()
    {
        return !Directory.Exists("Runtime/NotForLicensees");
    }

	public UnrealTournamentTarget(TargetInfo Target)
	{
        Type = TargetType.Game;

        bUsesCEF3 = true;

        // Turn on shipping checks and logging
        UEBuildConfiguration.bUseLoggingInShipping = true;
        UEBuildConfiguration.bUseChecksInShipping = true;
        UEBuildConfiguration.bCompileBox2D = false;
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
		OutExtraModuleNames.Add("UnrealTournamentFullScreenMovie");
		if (UEBuildConfiguration.bBuildEditor)
        {
            OutExtraModuleNames.Add("UnrealTournamentEditor");
        }

        if (!IsLicenseeBuild())
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
		UEBuildConfiguration.bWithPerfCounters = true;
    }
    
    public override bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
    {
        return false;
    }
}
