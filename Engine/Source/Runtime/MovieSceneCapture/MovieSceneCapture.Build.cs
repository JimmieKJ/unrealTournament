// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneCapture : ModuleRules
{
	public MovieSceneCapture(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/MovieSceneCapture/Private"
			}
		);

		if (UEBuildConfiguration.bBuildDeveloperTools)
		{
			PrivateIncludePathModuleNames.Add("ImageWrapper");
			DynamicallyLoadedModuleNames.Add("ImageWrapper");
		}

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"LevelSequence",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"MovieScene",
				"Slate",
				"SlateCore",
			}
		);
	}
}
