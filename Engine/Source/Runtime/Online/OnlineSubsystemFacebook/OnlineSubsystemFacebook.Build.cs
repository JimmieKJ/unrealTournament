// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemFacebook : ModuleRules
{
    public OnlineSubsystemFacebook(TargetInfo Target)
	{
		Definitions.Add("ONLINESUBSYSTEMFACEBOOK_PACKAGE=1");

		PrivateIncludePaths.Add("Runtime/Online/OnlineSubsystemFacebook/Private");

        PrivateDependencyModuleNames.AddRange(
            new string[] { 
                "Core", 
                "HTTP",
				"ImageCore",
				"Json",
                "OnlineSubsystem", 
            }
            );

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
			AddThirdPartyPrivateStaticDependencies(Target, "Facebook");

            PrivateIncludePaths.Add("Runtime/Online/OnlineSubsystemFacebook/Private/IOS");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateIncludePaths.Add("Runtime/Online/OnlineSubsystemFacebook/Private/Windows");
        }
	}
}
