// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemUtils : ModuleRules
{
	public OnlineSubsystemUtils(TargetInfo Target)
	{
		Definitions.Add("ONLINESUBSYSTEMUTILS_PACKAGE=1");

        PrivateIncludePaths.Add("Runtime/Online/OnlineSubsystemUtils/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject",
				"Engine", 
				"EngineSettings",
                "ImageCore",
				"OnlineSubsystem",
				"Sockets",
				"Voice",
                "PacketHandler",
				"Json"
			}
		);
	}
}
