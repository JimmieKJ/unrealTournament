// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;
using System.IO;

public class UnrealTournamentServerTarget : TargetRules
{
    bool IsLicenseeBuild()
    {
        return !Directory.Exists("Runtime/NotForLicensees");
    }

    public UnrealTournamentServerTarget(TargetInfo Target)
	{
        Type = TargetType.Server;
        bUsesSlate = false;

        // Turn on shipping logging
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
        if (!IsLicenseeBuild())
        {
            OutExtraModuleNames.Add("OnlineSubsystemMcp");
        }
    }

    public override bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
    {
        return false;
    }
}
