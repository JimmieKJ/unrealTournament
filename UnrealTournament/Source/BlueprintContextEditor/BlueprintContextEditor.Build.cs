// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BlueprintContextEditor : ModuleRules
{
    public BlueprintContextEditor(TargetInfo Target)
    {
        PublicDependencyModuleNames.AddRange( new string[] {
			"Core",
			"CoreUObject",
			"Engine",
            "Slate",
            "SlateCore",
            "UMG",
            "BlueprintContext",
		} );

        PrivateDependencyModuleNames.AddRange( new string[] {
            "UnrealEd",
            "GraphEditor",
            "KismetCompiler",
            "BlueprintGraph",
            "InputCore",
        } );

        PublicIncludePaths.AddRange( new string[] {
			"BlueprintContextEditor/Public",
        } );

        PrivateIncludePaths.AddRange( new string[] {
			"BlueprintContextEditor/Private",
        } );

		// Game modules usually are forced to get their own PCH and use non-unity if they have less than a certain number
		// of source files.  We don't want to slow down full builds for our game modules which are not frequently-iterated on
		MinSourceFilesForUnityBuildOverride = 1;
		MinFilesUsingPrecompiledHeaderOverride = BuildConfiguration.MinFilesUsingPrecompiledHeader;

		// This module doesn't depend on big header files from this game, so allow it to use an engine PCH for much faster compile times!
		PCHUsage = PCHUsageMode.UseSharedPCHs;
	}
}