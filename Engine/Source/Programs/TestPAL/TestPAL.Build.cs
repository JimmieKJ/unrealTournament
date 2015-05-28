// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TestPAL : ModuleRules
{
	public TestPAL(TargetInfo Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Projects",
				"DirectoryWatcher"
			}
		);
	}
}
