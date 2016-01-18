// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetTools : ModuleRules
{
	public AssetTools(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Developer/AssetTools/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "CurveAssetEditor",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"SourceControl",
				"TextureEditor",
				"UnrealEd",
				"Kismet",
				"Landscape",
                "Foliage",
                "Niagara",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics",
				"AssetRegistry",
				"ContentBrowser",
				"CollectionManager",
                "CurveAssetEditor",
				"DesktopPlatform",
				"EditorWidgets",
				"GameProjectGeneration",
				"Kismet",
				"MainFrame",
				"MaterialEditor",
				"Persona",
				"FontEditor",
				"SoundCueEditor",
				"SoundClassEditor",
				"SourceControl",
				"Landscape",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"ContentBrowser",
				"CollectionManager",
				"CurveTableEditor",
				"DataTableEditor",
				"DesktopPlatform",
				"DestructibleMeshEditor",
				"EditorWidgets",
				"GameProjectGeneration",
				"PropertyEditor",
				"MainFrame",
				"MaterialEditor",
				"Persona",
				"FontEditor",
				"SoundCueEditor",
				"SoundClassEditor"
			}
		);
	}
}
