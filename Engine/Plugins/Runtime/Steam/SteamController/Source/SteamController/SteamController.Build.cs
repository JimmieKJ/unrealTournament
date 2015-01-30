// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SteamController : ModuleRules
{
    public SteamController(TargetInfo Target)
    {
        PrivateIncludePathModuleNames.Add("TargetPlatform");

        PrivateDependencyModuleNames.AddRange(new string[]
        {
			"Core",
			"CoreUObject",
			"Engine",
			"InputDevice",
		});

        AddThirdPartyPrivateStaticDependencies(Target,
            "Steamworks"
        );
    }
}
