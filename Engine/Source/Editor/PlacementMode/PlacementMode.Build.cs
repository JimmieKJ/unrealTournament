// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PlacementMode : ModuleRules
{
    public PlacementMode(TargetInfo Target)
    {
        PrivateIncludePathModuleNames.Add("AssetTools");

        PrivateDependencyModuleNames.AddRange( 
            new string[] { 
                "Core", 
                "CoreUObject",
                "Engine", 
                "InputCore",
                "Slate",
				"SlateCore",
                "UnrealEd",
                "ContentBrowser",
                "CollectionManager",
                "LevelEditor",
                "AssetTools",
                "EditorStyle",
				"BspMode"
            } 
        );
    }
}
