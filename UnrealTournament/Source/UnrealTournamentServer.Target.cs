// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealTournamentServerTarget : TargetRules
{
    public UnrealTournamentServerTarget(TargetInfo Target)
	{
        Type = TargetType.Server;
        bUsesSlate = false;
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
	}
}
