// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StreamingPauseRendering : ModuleRules
{
    public StreamingPauseRendering(TargetInfo Target)
	{
        PrivateIncludePaths.Add("Runtime/StreamingPauseRendering/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
					"Engine",
                    "MoviePlayer",
                    "Slate",
				}
		);

		PrivateDependencyModuleNames.AddRange(
            new string[] {
                    "Core",
                    "InputCore",
                    "RenderCore",
                    "CoreUObject",
                    "RHI",
					"SlateCore",
				}
        );
	}
}
