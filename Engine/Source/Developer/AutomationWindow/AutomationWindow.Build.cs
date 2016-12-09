// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AutomationWindow : ModuleRules
	{
		public AutomationWindow(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"AutomationController",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "DesktopPlatform",
                    "InputCore",
                    "Slate",
                    "SlateCore",
                    "EditorStyle",
                    "CoreUObject",
                    "Json",
                    "JsonUtilities"
                }
			);

            // Added more direct dependencies to the editor for testing functionality
            if (UEBuildConfiguration.bBuildEditor)
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[] {
                        "UnrealEd",
                        "Engine", // Needed for UWorld/GWorld to find current level
				    }
                );
            }

            PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"SessionServices",
				}
			);
		}
	}
}
