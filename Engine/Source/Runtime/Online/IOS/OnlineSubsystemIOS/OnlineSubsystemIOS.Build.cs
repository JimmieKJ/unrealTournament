// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemIOS : ModuleRules
{
    public OnlineSubsystemIOS(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange( 
            new string[] {
                "Runtime/Online/IOS/OnlineSubsystemIOS/Private",             
                }
                );

        Definitions.Add("ONLINESUBSYSTEMIOS_PACKAGE=1");

		PrivateDependencyModuleNames.AddRange(
            new string[] { 
                "Core", 
                "CoreUObject", 
                "Engine", 
                "Sockets",
				"OnlineSubsystem", 
                "Http",
            }
            );

		PublicWeakFrameworks.Add("Cloudkit");
		PublicWeakFrameworks.Add("MultipeerConnectivity");
	}
}
