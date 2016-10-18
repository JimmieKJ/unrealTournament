// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
				"EditorWidgets",
				"UnrealEd",
				"AssetRegistry",
				"ClassViewer",
                "Kismet",
				"KismetWidgets",
				"BlueprintGraph",
				"AnimGraph",
				"Documentation",
				"RenderCore",
				"RHI",
			}
		);

		// TODO: Move niagara code to niagara modules.  This is temporarily necessary to fix the public include issues.
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"NiagaraEditor"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"ContentBrowser",
			}
		);
	}
}
