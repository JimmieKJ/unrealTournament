// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
                }
            );

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Settings",
				}
			);
		}
	}
}
