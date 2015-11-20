// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorldBrowser : ModuleRules
{
    public WorldBrowser(TargetInfo Target)
    {
        PrivateIncludePaths.Add("Editor/WorldBrowser/Private");	// For PCH includes (because they don't work with relative paths, yet)

        PrivateIncludePathModuleNames.AddRange(
        new string[] {
                "AssetRegistry",
				"AssetTools",
                "ContentBrowser",
				"Landscape",
                "MeshUtilities",
                "MaterialUtilities",
			}
        );
     
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "AppFramework",
                "Core", 
                "CoreUObject",
                "RenderCore",
                "InputCore",
                "Engine",
				"Landscape",
                "Slate",
				"SlateCore",
                "EditorStyle",
                "UnrealEd",
                "GraphEditor",
                "LevelEditor",
                "PropertyEditor",
                "DesktopPlatform",
                "MainFrame",
                "SourceControl",
				"SourceControlWindows",
                "RawMesh",
                "LandscapeEditor",
                "ImageWrapper",
                "Foliage",
                "MaterialUtilities",
                "RHI"
            }
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
				"AssetTools",
				"SceneOutliner",
                "MeshUtilities",
                "ContentBrowser",
			}
		);
    }
}
