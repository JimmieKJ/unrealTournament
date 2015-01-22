// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrashDebugHelper : ModuleRules
{
	public CrashDebugHelper(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Developer/CrashDebugHelper/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"SourceControl",
				"DatabaseSupport"
			}
		);
	}
}
