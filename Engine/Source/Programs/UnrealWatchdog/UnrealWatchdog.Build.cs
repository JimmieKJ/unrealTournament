// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealWatchdog : ModuleRules
{
	public UnrealWatchdog(TargetInfo Target)
	{
		PublicIncludePaths.AddRange
		(
			new string[]
			{
				"Runtime/Launch/Public",
				"Programs/UnrealWatchdog/Private",
			}
		);

		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Runtime/Launch/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
				"Analytics",
				"AnalyticsET",
				"Projects",
		   }
		);
	}
}
