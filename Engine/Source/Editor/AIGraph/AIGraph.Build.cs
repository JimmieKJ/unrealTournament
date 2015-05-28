// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AIGraph : ModuleRules
{
    public AIGraph(TargetInfo Target)
    {
        PrivateIncludePaths.AddRange(
            new string[] {
				"Editor/GraphEditor/Private",
				"Editor/Kismet/Private",
				"Editor/AIGraph/Private",
			}
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser"
			}
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core", 
				"CoreUObject", 
				"Engine", 
                "RenderCore",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd", 
				"MessageLog", 
				"GraphEditor",
                "Kismet",
				"AnimGraph",
				"BlueprintGraph",
                "AIModule",
				"ClassViewer"
			}
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[] { 
				"AssetTools",
				"AssetRegistry",
				"ContentBrowser"
            }
        );
    }
}
