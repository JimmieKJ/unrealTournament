// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameProjectGeneration : ModuleRules
{
    public GameProjectGeneration(TargetInfo Target)
	{
        PrivateIncludePaths.AddRange(new string[] { "GameProjectGeneration/Private", "GameProjectGeneration/Public", "GameProjectGeneration/Classes" });


        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"ContentBrowser",
                "DesktopPlatform",
                "MainFrame",
            }
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
                "AppFramework",
				"ClassViewer",
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
                "HardwareTargeting",
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"ContentBrowser",
                "DesktopPlatform",
                "Documentation",
                "MainFrame",
            }
		);
	}
}
