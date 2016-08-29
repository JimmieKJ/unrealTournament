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
    
    public override bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
    {
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
        // ORDER HERE MATTERS, THE FIRST ENTRY IS PUT IN Manifest_NonUFSFiles.txt AND THE FOLLOWING ARE PUT IN Manifest_DebugFiles.txt
        return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Shipping, UnrealTargetConfiguration.Test };
    }
}
