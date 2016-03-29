// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;

public class FakeIt : ModuleRules
{
    public FakeIt(TargetInfo Target)
	{
		Type = ModuleType.External;

		string Version = "2.0.2";
		string RootPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "FakeIt/" + Version + "/";

		// Includes
        PublicSystemIncludePaths.Add(RootPath + "config/standalone");
        PublicSystemIncludePaths.Add(RootPath + "include");

        // The including module will also need these enabled
        bUseRTTI = true;
        bEnableExceptions = true;
        Definitions.Add("WITH_FAKEIT=1");
    }
}
