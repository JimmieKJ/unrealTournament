// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InternationalizationSettings : ModuleRules
	{
        public InternationalizationSettings(TargetInfo Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[] {
				    "Core",
				    "CoreUObject",
				    "InputCore",
				    "Engine",
				    "Slate",
					"SlateCore",
				    "EditorStyle",
				    "PropertyEditor",
				    "SharedSettingsWidgets",
                    "Localization",
                }
            );

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Settings",
                    "SettingsEditor"
				}
			);

            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
				    "SettingsEditor"
                }
            );
		}
	}
}
