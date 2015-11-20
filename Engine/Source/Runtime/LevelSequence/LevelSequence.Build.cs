// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelSequence : ModuleRules
{
	public LevelSequence(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/LevelSequence/Private");

		PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
				"MovieScene",
            }
        );
	}
}
