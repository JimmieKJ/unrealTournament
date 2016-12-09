// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioEditor : ModuleRules
{
    public AudioEditor(TargetInfo Target)
    {
        PrivateIncludePaths.AddRange(
            new string[] {
                "Editor/AudioEditor/Private",
                "Editor/AudioEditor/Private/Factories",
                "Editor/AudioEditor/Private/AssetTypeActions"
            });

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "InputCore",
                "Engine",
                "UnrealEd",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "RenderCore",
                "LevelEditor",
                "Landscape",
                "PropertyEditor",
                "DetailCustomizations",
//                "AssetTools",
                "GraphEditor",
                "ContentBrowser",
            }
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools"
			});
	}
}
