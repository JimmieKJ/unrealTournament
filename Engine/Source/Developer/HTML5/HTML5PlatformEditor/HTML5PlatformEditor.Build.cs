// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTML5PlatformEditor : ModuleRules
{
	public HTML5PlatformEditor(TargetInfo Target)
	{
		BinariesSubFolder = "HTML5";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
				"EditorStyle",
                "EditorWidgets",
                "DesktopWidgets",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"SourceControl",
                "TargetPlatform",
				"HTML5TargetPlatform",
				// WITH_EDITOR only?
				"UnrealEd",
				"DesktopPlatform",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);
	}
}
