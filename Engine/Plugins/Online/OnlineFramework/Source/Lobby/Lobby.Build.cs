// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Lobby : ModuleRules
{
	public Lobby(TargetInfo Target)
	{
		Definitions.Add("LOBBY_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64)
		{
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
		}
	}
}