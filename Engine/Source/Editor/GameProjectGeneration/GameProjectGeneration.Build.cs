// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameProjectGeneration : ModuleRules
{
    public GameProjectGeneration(TargetInfo Target)
	{
        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"ClassViewer",
                "DesktopPlatform",
                "MainFrame",
            }
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
                "AppFramework",
				"Core",
				"CoreUObject",
				"Engine",
				"EngineSettings",
                "InputCore",
				"Projects",
                "RenderCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "SourceControl",
 				"TargetPlatform",
				"UnrealEd",
				"DesktopPlatform",
                "HardwareTargeting"
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"ClassViewer",
                "DesktopPlatform",
                "Documentation",
                "MainFrame",
            }
		);
	}
}
