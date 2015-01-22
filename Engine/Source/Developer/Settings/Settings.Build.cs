// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Settings : ModuleRules
{
	public Settings(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
				"CoreUObject",
            }
        );

		PrivateIncludePaths.AddRange(
			new string[]
            {
                "Developer/Settings/Private",
            }
		);
	}
}
