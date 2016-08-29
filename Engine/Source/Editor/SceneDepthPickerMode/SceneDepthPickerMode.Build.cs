// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SceneDepthPickerMode : ModuleRules
{
    public SceneDepthPickerMode(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
                "Slate",
                "SlateCore",
                "EditorStyle",
				"UnrealEd",
			}
		);
	}
}
