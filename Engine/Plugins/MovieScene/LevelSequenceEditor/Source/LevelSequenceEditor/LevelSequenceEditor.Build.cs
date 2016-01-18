// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelSequenceEditor : ModuleRules
{
	public LevelSequenceEditor(TargetInfo Target)
	{
        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"AssetTools",
				"SceneOutliner",
			}
        );
        
        PrivateDependencyModuleNames.AddRange(
			new string[] {
                "LevelSequence",
				"BlueprintGraph",
				"Core",
				"CoreUObject",
                "EditorStyle",
                "Engine",
                "InputCore",
				"LevelEditor",
				"MovieScene",
				"MovieSceneTracks",
                "PropertyEditor",
				"Sequencer",
                "Slate",
                "SlateCore",
                "UnrealEd",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"SceneOutliner",
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
				"LevelSequenceEditor/Private",
				"LevelSequenceEditor/Private/AssetTools",
                "LevelSequenceEditor/Private/Customizations",
				"LevelSequenceEditor/Private/Factories",
				"LevelSequenceEditor/Private/Styles",
			}
        );
	}
}
