// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
            PrivateDependencyModuleNames.Add("Kismet");
            PrivateDependencyModuleNames.Add("GraphEditor");
            PrivateDependencyModuleNames.Add("BlueprintGraph");
            PrivateDependencyModuleNames.Add("EditorStyle");
            PrivateDependencyModuleNames.Add("InputCore");
            PrivateDependencyModuleNames.Add("Slate");
            PrivateDependencyModuleNames.Add("SlateCore");
        }
    }
}

