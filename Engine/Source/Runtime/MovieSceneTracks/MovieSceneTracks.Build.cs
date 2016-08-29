// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneTracks : ModuleRules
{
	public MovieSceneTracks(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(new string[]
        { 
            "Runtime/MovieSceneTracks/Private",
			"Runtime/MovieSceneTracks/Private/Sections",
			"Runtime/MovieSceneTracks/Private/Tracks",
            "Runtime/MovieSceneTracks/Private/TrackInstances",
        });

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"MovieScene",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SlateCore",
			}
		);
	}
}
