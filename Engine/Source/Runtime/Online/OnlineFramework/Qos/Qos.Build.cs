// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Qos : ModuleRules
{
	public Qos(TargetInfo Target)
	{
		Definitions.Add("QOS_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Online/OnlineFramework/Qos/Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject",
				"Engine", 
				"OnlineSubsystem",
				"OnlineSubsystemUtils"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"Analytics",
				"AnalyticsET"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics"
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