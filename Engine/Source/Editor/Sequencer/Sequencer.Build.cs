// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sequencer : ModuleRules
{
	public Sequencer(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(
            new string[] {
                "Editor/Sequencer/Private",
                "Editor/Sequencer/Private/DisplayNodes",
				"Editor/UnrealEd/Private" // TODO: Fix this, for now it's needed for the fbx exporter
	}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AppFramework", 
                "CinematicCamera",
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
                "ContentBrowser",
				"PropertyEditor",
				"Kismet",
				"SceneOutliner",
				"SequenceRecorder",
                "LevelEditor",
				"MainFrame",
				"DesktopPlatform"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				"LevelEditor",
				"SceneOutliner",
				"WorkspaceMenuStructure",
				"SequenceRecorder",
				"SequenceRecorderSections",
				"MainFrame",
			}
		);

		CircularlyReferencedDependentModules.Add("MovieSceneTools");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");
	}
}
