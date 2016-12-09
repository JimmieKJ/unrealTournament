// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WidgetCarousel : ModuleRules
{
	public WidgetCarousel(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Slate",
				"SlateCore",
				"InputCore",
				"CoreUObject"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/WidgetCarousel/Private",
			}
		);
	}
}
