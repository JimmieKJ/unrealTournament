// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneTools : ModuleRules
{
	public MovieSceneTools(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(
            new string[] {
                "Editor/MovieSceneTools/Private",
                "Editor/MovieSceneTools/Private/CurveKeyEditors",
                "Editor/MovieSceneTools/Private/TrackEditors",
				"Editor/MovieSceneTools/Private/TrackEditors/PropertyTrackEditors",
                "Editor/MovieSceneTools/Private/TrackEditors/ShotTrackEditor",
				"Editor/MovieSceneTools/Private/Sections"
            }
        );

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"UnrealEd",
				"Sequencer"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "ActorPickerMode",
				"MovieScene",
				"MovieSceneTracks",
				"BlueprintGraph",
                "ContentBrowser",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"RenderCore",
				"RHI"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"Sequencer",
				"SceneOutliner",
				"PropertyEditor"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"SceneOutliner",
				"PropertyEditor"
			}
		);

        bFasterWithoutUnity = true;
	}
}
