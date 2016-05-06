// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BlueprintContext : ModuleRules
{
    public BlueprintContext(TargetInfo Target)
    {
		PublicIncludePathModuleNames.Add("Social");
        
        PublicDependencyModuleNames.AddRange( new string[] {
			"Core",
			"CoreUObject",
			"Engine",
            "Slate",
            "SlateCore",
            "UMG",
            "UnrealTournament",
            "OnlineSubsystem",
			"GameplayTags",
            "Party"
		});

        if (UEBuildConfiguration.bCompileMcpOSS == true)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
			        "McpProfileSys",
                    "Social",
                    "OnlineSubsystemMcp",
			        "GameSubCatalog"
                }
            );
        }

        PrivateDependencyModuleNames.AddRange(new string[] {
            "OnlineSubsystemUtils",
            "InputCore",
            "GameplayAbilities",
			"Qos",
			"Json",
        } );

        PublicIncludePaths.AddRange( new string[] {
			"BlueprintContext/Public",
        } );

        PrivateIncludePaths.AddRange( new string[] {
			"BlueprintContext/Private",
        } );

		// Game modules usually are forced to get their own PCH and use non-unity if they have less than a certain number
		// of source files.  We don't want to slow down full builds for our game modules which are not frequently-iterated on
		MinSourceFilesForUnityBuildOverride = 1;
		MinFilesUsingPrecompiledHeaderOverride = BuildConfiguration.MinFilesUsingPrecompiledHeader;
	}
}
