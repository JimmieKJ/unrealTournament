// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CinematicCamera : ModuleRules
{
	public CinematicCamera(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/CinematicCamera/Private"
            })
		;
	}
}
