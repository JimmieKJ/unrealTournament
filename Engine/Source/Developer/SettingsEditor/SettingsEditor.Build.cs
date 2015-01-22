// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SettingsEditor : ModuleRules
{
	public SettingsEditor(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

		PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Analytics",
                "CoreUObject",
				"DesktopPlatform",
                "EditorStyle",
				"Engine",
                "InputCore",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"Slate",
				"SlateCore",
				"SourceControl",
            }
        );

		PrivateIncludePaths.AddRange(
			new string[] {
                "Developer/SettingsEditor/Private",
				"Developer/SettingsEditor/Private/Models",
                "Developer/SettingsEditor/Private/Widgets",
            }
		);
	}
}
