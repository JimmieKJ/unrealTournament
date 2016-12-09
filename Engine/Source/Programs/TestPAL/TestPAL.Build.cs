// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TestPAL : ModuleRules
{
	public TestPAL(TargetInfo Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Programs/TestPAL/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Projects",
				"DirectoryWatcher"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SDL2",
				}
			);
		}
	}
}
