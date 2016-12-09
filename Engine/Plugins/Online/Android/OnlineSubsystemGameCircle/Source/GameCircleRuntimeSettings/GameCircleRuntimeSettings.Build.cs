// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameCircleRuntimeSettings : ModuleRules
{
	public GameCircleRuntimeSettings(TargetInfo Target)
	{
		BinariesSubFolder = "Android";

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

        if (Target.Type == TargetRules.TargetType.Editor || Target.Type == TargetRules.TargetType.Program)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
			    {
					"Settings",
					"TargetPlatform",
                    "Android_MultiTargetPlatform"
			    }
            );
        }
	}
}
