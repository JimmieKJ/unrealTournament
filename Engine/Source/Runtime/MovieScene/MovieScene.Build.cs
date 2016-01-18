// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieScene : ModuleRules
{
	public MovieScene(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/MovieScene/Private");

		PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );
	}
}
