// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BuildPatchTool : ModuleRules
{
	public BuildPatchTool( TargetInfo Target )
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Runtime/Launch/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{
				"Core",
				"BuildPatchServices",
				"Projects",
				// The below items are not strictly needed by BPT, but core appears to need them during initialization
				"PakFile",
				"SandboxFile",
				"NetworkFile",
				"StreamingFile"
			}
		);
	}
}
