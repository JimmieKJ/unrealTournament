// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GitSourceControl : ModuleRules
{
	public GitSourceControl(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorStyle",
				"SourceControl",
			}
		);

		if (UEBuildConfiguration.bBuildEditor == true)
		{
			// needed to enable/disable this via experimental settings
			PrivateDependencyModuleNames.Add("CoreUObject");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
