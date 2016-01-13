// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealTournamentEditorTarget : TargetRules
{
	public UnrealTournamentEditorTarget(TargetInfo Target)
	{
		Type = TargetType.Editor;
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
        OutExtraModuleNames.Add("UnrealTournamentEditor");

        if (UEBuildConfiguration.bCompileMcpOSS == true)
        {
            OutExtraModuleNames.Add("OnlineSubsystemMcp");
        }
        OutExtraModuleNames.Add("OnlineSubsystemNull");
	}
    public override GUBPProjectOptions GUBP_IncludeProjectInPromotedBuild_EditorTypeOnly(UnrealTargetPlatform HostPlatform)
    {
        var Result = new GUBPProjectOptions();
        Result.bIsPromotable = true;
        Result.bSeparateGamePromotion = true;
        Result.bCustomWorkflowForPromotion = true;
        return Result;
    }
}
