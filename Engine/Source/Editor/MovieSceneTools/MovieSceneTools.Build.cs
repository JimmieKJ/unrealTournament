// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
                "Editor/MovieSceneTools/Private/TrackEditorThumbnail",
				"Editor/MovieSceneTools/Private/Sections",
                "Editor/UnrealEd/Private",	//compatibility for FBX importer
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
                "DesktopPlatform",
				"LevelSequence",
				"MovieScene",
                "MovieSceneCapture",
				"MovieSceneTracks",
				"BlueprintGraph",
                "ContentBrowser",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"EditorWidgets",
				"RenderCore",
				"RHI",
				"ShaderCore",
				"SequenceRecorder",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
                "AssetRegistry",
				"AssetTools",
				"Sequencer",
                "Settings",
				"SceneOutliner",
				"PropertyEditor",
                "MainFrame",
                "UnrealEd",
            }
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
                "AssetRegistry",
				"AssetTools",
				"SceneOutliner",
				"PropertyEditor",
                "MainFrame",
			}
		);

        CircularlyReferencedDependentModules.AddRange(
            new string[] {
                "Sequencer",
            }
        );

        AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");
    }
}
