// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SmartSnapping : ModuleRules
{
	public SmartSnapping(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"SlateCore",
				"Slate",
				"LevelEditor",
				"ViewportSnapping"
			});
	}
}
