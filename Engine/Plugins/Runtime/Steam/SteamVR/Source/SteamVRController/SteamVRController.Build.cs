// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SteamVRController : ModuleRules
{
    public SteamVRController(TargetInfo Target)
    {
        PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"TargetPlatform",
            "SteamVR"
		});

        PrivateDependencyModuleNames.AddRange(new string[]
        {
			"Core",
			"CoreUObject",
			"Engine",
			"InputDevice",
			"HeadMountedDisplay",
            "SteamVR"
		});

// 		DynamicallyLoadedModuleNames.AddRange(new string[]
// 		{
// 			"SteamVR",
// 		});

		AddThirdPartyPrivateStaticDependencies(Target,
            "OpenVR"
        );
    }
}
