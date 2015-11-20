// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LocalizationDashboard : ModuleRules
{
	public LocalizationDashboard(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
                "PropertyEditor",
                "Localization"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
                "UnrealEd",
                "EditorStyle",
				"DesktopPlatform",
                "TranslationEditor",
                "MainFrame",
                "SourceControl",
                "SharedSettingsWidgets",
                "Localization",
                "LocalizationService"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Editor/LocalizationDashboard/Private",
			}
		);

        PublicIncludePaths.AddRange(
			new string[]
			{
				"Editor/LocalizationDashboard/Public",
			}
		);

        CircularlyReferencedDependentModules.AddRange(
           new string[] {
                "LocalizationService",
				"MainFrame",
				"TranslationEditor"
            }
           );
	}
}