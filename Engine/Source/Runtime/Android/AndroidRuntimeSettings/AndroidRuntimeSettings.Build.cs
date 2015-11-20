// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AndroidRuntimeSettings : ModuleRules
{
	public AndroidRuntimeSettings(TargetInfo Target)
	{
		BinariesSubFolder = "Android";

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject"
			}
		);

        if (Target.Type == TargetRules.TargetType.Editor || Target.Type == TargetRules.TargetType.Program)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
			    {
                    "TargetPlatform",
                    "Android_MultiTargetPlatform"
			    }
            );
        }
	}
}
