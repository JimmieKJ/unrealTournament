// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealTournamentServerTarget : TargetRules
{
    public UnrealTournamentServerTarget(TargetInfo Target)
	{
        Type = TargetType.Server;
        bUsesSlate = false;

        // Turn on shipping logging
        UEBuildConfiguration.bUseLoggingInShipping = true;
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
        if (UEBuildConfiguration.bCompileMcpOSS == true)
        {
            OutExtraModuleNames.Add("OnlineSubsystemMcp");
        }
    }

    public override bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
    {
        return false;
    }

    public override List<UnrealTargetPlatform> GUBP_GetPlatforms_MonolithicOnly(UnrealTargetPlatform HostPlatform)
    {
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            return new List<UnrealTargetPlatform>();
        }
        return new List<UnrealTargetPlatform> { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Linux };
    }

    public override List<UnrealTargetConfiguration> GUBP_GetConfigs_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
    {
        // ORDER HERE MATTERS, THE FIRST ENTRY IS PUT IN Manifest_NonUFSFiles.txt AND THE FOLLOWING ARE PUT IN Manifest_DebugFiles.txt
        return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Test };
    }
}
