// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MediaPlayerEditor : ModuleRules
{
	public MediaPlayerEditor(TargetInfo Target)
	{
		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
                "AssetTools",
				"MainFrame",
                "Media",
				"WorkspaceMenuStructure",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"MediaPlayerEditor/Private",
                "MediaPlayerEditor/Private/AssetTools",
                "MediaPlayerEditor/Private/Factories",
                "MediaPlayerEditor/Private/Models",
                "MediaPlayerEditor/Private/Styles",
                "MediaPlayerEditor/Private/Widgets",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "ContentBrowser",
				"Core",
				"CoreUObject",
                "EditorStyle",
				"Engine",
                "InputCore",
                "MediaAssets",
                "PropertyEditor",
				"RenderCore",
				"RHI",
				"ShaderCore",
				"Slate",
				"SlateCore",
                "TextureEditor",
				"UnrealEd"
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetTools",
                "Media",
				"UnrealEd",
                "WorkspaceMenuStructure",
			}
        );
    }
}
