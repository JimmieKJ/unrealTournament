// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemUtils : ModuleRules
{
	public OnlineSubsystemUtils(TargetInfo Target)
	{
		Definitions.Add("ONLINESUBSYSTEMUTILS_PACKAGE=1");

        PrivateIncludePaths.Add("OnlineSubsystemUtils/Private");

        PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject",
				"Engine", 
				"EngineSettings",
                "ImageCore",
				"Sockets",
				"Voice",
                "PacketHandler",
				"Json"
			}
		);

        PublicDependencyModuleNames.Add("OnlineSubsystem");
	}
}
