// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemFacebook : ModuleRules
{
    public OnlineSubsystemFacebook(TargetInfo Target)
	{
		Definitions.Add("ONLINESUBSYSTEMFACEBOOK_PACKAGE=1");

        PrivateIncludePaths.Add("Private");

        PrivateDependencyModuleNames.AddRange(
            new string[] { 
                "Core",
                "CoreUObject",
                "HTTP",
				"ImageCore",
				"Json",
                "OnlineSubsystem", 
            }
            );

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Facebook");

            PrivateIncludePaths.Add("Private/IOS");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateIncludePaths.Add("Private/Windows");
        }
		else
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
