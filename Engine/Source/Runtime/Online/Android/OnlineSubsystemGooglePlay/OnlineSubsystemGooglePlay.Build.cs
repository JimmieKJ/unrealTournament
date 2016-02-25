// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemGooglePlay : ModuleRules
{
	public OnlineSubsystemGooglePlay(TargetInfo Target)
	{
		Definitions.Add("ONLINESUBSYSTEMGOOGLEPLAY_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Online/Android/OnlineSubsystemGooglePlay/Private",    
				"Runtime/Launch/Private"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
                "Core", 
                "CoreUObject", 
                "Engine", 
                "Sockets",
				"OnlineSubsystem", 
                "Http",
				"AndroidRuntimeSettings",
				"Launch",
				"GpgCppSDK"
            }
			);
	}
}
