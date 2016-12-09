// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MoviePlayer : ModuleRules
{
	public MoviePlayer(TargetInfo Target)
	{
        PrivateIncludePaths.Add("Runtime/MoviePlayer/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
					"Engine",
				}
		);

		PrivateDependencyModuleNames.AddRange(
            new string[] {
                    "Core",
                    "InputCore",
                    "RenderCore",
                    "CoreUObject",
                    "RHI",
                    "Slate",
					"SlateCore",
                    "HeadMountedDisplay"
				}
        );
	}
}
