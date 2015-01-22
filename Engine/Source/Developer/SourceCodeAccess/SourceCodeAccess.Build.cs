// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SourceCodeAccess : ModuleRules
{
	public SourceCodeAccess(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Settings",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Slate",
			}
		);

		PrivateIncludePaths.Add("Developer/SourceCodeAccess/Private");
	}
}
