// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GraphEditor : ModuleRules
{
	public GraphEditor(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/GraphEditor/Private",
				"Editor/GraphEditor/Private/AnimationPins",
				"Editor/GraphEditor/Private/AnimationStateNodes",
				"Editor/GraphEditor/Private/KismetNodes",
				"Editor/GraphEditor/Private/KismetPins",
				"Editor/GraphEditor/Private/MaterialNodes",
				"Editor/GraphEditor/Private/MaterialPins",
				"Editor/GraphEditor/Private/SoundNodes",
			}
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {                
                "IntroTutorials"
            }
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] { 
				"EditorWidgets"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"AssetRegistry",
				"ClassViewer",
				"KismetWidgets",
				"BlueprintGraph",
				"AnimGraph",
				"Documentation",
				"RenderCore",
				"RHI",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"ContentBrowser",
				"EditorWidgets"
			}
		);
	}
}
