// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SpriterImporter : ModuleRules
{
	public SpriterImporter(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"Paper2D",
				"UnrealEd",
				"Paper2DEditor"
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"AssetRegistry"
			});

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"AssetRegistry"
			});
	}
}
