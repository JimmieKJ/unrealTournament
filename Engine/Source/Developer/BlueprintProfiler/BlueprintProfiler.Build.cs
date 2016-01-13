// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BlueprintProfiler : ModuleRules
{
    public BlueprintProfiler(TargetInfo Target)
    {
		PrivateIncludePaths.Add("Private");

        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"EngineSettings",
			}
            );

        if (UEBuildConfiguration.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }

        if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
        {
            // work around for bad naming of "classes" directory, proper resolution is tracked as UE-23934
            bFasterWithoutUnity = true;
        }
    }
}

