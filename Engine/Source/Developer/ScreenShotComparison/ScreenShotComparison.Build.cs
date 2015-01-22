// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ScreenShotComparison : ModuleRules
{
	public ScreenShotComparison(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "EditorStyle",
                "InputCore",
				"ScreenShotComparisonTools",
				"Slate",
				"SlateCore",
				"ImageWrapper",
				"CoreUObject",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"SessionServices",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/ScreenShotComparison/Private",
				"Developer/ScreenShotComparison/Private/Widgets",
				"Developer/ScreenShotComparison/Private/Models",
			}
		);
	}
}
