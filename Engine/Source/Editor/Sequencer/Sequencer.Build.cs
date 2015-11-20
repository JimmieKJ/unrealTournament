// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sequencer : ModuleRules
{
	public Sequencer(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(
            new string[] {
                "Editor/Sequencer/Private",
                "Editor/Sequencer/Private/DisplayNodes",
            }
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
                "InputCore",
				"Engine", 
				"Slate", 
				"SlateCore",
                "EditorStyle",
				"UnrealEd", 
				"MovieScene", 
				"MovieSceneTracks", 
				"MovieSceneTools", 
				"MovieSceneCapture", 
                "MovieSceneCaptureDialog", 
				"EditorWidgets", 
				"SequencerWidgets",
				"BlueprintGraph",
				"LevelSequence"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				"Kismet",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				"LevelEditor",
				"WorkspaceMenuStructure",
			}
		);

		CircularlyReferencedDependentModules.Add("MovieSceneTools");
	}
}
