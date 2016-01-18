// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ContentBrowser : ModuleRules
{
	public ContentBrowser(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"AssetTools",
				"CollectionManager",
				"EditorWidgets",
				"GameProjectGeneration",
                "MainFrame",
				"SourceControl",
				"SourceControlWindows",
                "ReferenceViewer",
                "SizeMap",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"SourceControl",
				"SourceControlWindows",
				"WorkspaceMenuStructure",
				"UnrealEd",
				"EditorWidgets",
				"Projects",
				"AddContentDialog"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				"AssetRegistry",
				"AssetTools",
				"CollectionManager",
				"GameProjectGeneration",
                "MainFrame",
                "ReferenceViewer",
                "SizeMap",
			}
		);
		
		PublicIncludePathModuleNames.AddRange(
            new string[] {                
                "IntroTutorials"
            }
        );
	}
}
