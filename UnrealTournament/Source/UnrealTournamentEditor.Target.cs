// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;
using System.IO;

public class UnrealTournamentEditorTarget : TargetRules
{
    bool IsLicenseeBuild()
    {
        return !Directory.Exists("Runtime/NotForLicensees");
    }

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

        if (!IsLicenseeBuild())
        {
            OutExtraModuleNames.Add("OnlineSubsystemMcp");
        }
        OutExtraModuleNames.Add("OnlineSubsystemNull");
	}
}
