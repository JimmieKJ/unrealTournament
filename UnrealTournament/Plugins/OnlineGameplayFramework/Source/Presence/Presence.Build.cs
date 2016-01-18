// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Presence : ModuleRules
{
	public Presence(TargetInfo Target)
	{
		Definitions.Add("PRESENCE_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"OnlineGameplayFramework/Source/Presence/Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject"
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