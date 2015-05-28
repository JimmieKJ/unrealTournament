// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Kismet : ModuleRules
{
	public Kismet(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Editor/Kismet/Private");

		PrivateIncludePathModuleNames.AddRange(
			new string[] { 
				"AssetRegistry", 
				"AssetTools",
				"ClassViewer",
				"EditorWidgets",
				"Analytics",
                "DerivedDataCache",
                "LevelEditor",
				"GameProjectGeneration",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"Engine",
				"Json",
				"Merge",
				"MessageLog",
				"UnrealEd",
				"GraphEditor",
				"KismetWidgets",
				"BlueprintGraph",
				"AnimGraph",
				"PropertyEditor",
				"SourceControl",
                "InputCore",
				"EngineSettings",
                "Projects",
                "JsonUtilities",
                "DerivedDataCache",
				"DesktopPlatform",
				"HotReload",
			}
			);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"ClassViewer",
				"Documentation",
				"EditorWidgets",
				"GameProjectGeneration",
                "BlueprintCompilerCppBackend",
			}
            );
	}
}
