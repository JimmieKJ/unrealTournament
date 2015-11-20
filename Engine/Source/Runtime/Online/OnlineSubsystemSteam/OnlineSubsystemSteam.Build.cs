// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemSteam : ModuleRules
{
	public OnlineSubsystemSteam(TargetInfo Target)
	{
		Definitions.Add("ONLINESUBSYSTEMSTEAM_PACKAGE=1");

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"OnlineSubsystemUtils",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"Engine", 
				"Sockets", 
				"Voice",
				"OnlineSubsystem",
				"Json"
			}
		);

		AddThirdPartyPrivateStaticDependencies(Target, "Steamworks");
	}
}
