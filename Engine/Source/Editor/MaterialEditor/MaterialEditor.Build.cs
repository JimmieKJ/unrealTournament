// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MaterialEditor : ModuleRules
{
	public MaterialEditor(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] 
			{
				"AssetRegistry", 
				"AssetTools",
				"Kismet",
				"EditorWidgets"
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
				"ShaderCore",
				"RenderCore",
				"RHI",
				"UnrealEd",
				"PropertyEditor",
				"GraphEditor",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"MainFrame",
				"SceneOutliner",
				"ClassViewer",
				"ContentBrowser",
				"WorkspaceMenuStructure"
			}
		);
	}
}
