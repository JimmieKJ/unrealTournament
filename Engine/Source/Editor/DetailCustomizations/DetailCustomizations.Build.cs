// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DetailCustomizations : ModuleRules
{
	public DetailCustomizations(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Editor/DetailCustomizations/Private");	// For PCH includes (because they don't work with relative paths, yet)

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"DesktopWidgets",
				"Engine",
				"Landscape",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"EditorWidgets",
				"KismetWidgets",
				"SharedSettingsWidgets",
                "ContentBrowser",
				"BlueprintGraph",
				"AnimGraph",
                "PropertyEditor",
                "LevelEditor",
				"DesktopPlatform",
				"ClassViewer",
				"TargetPlatform",
				"ExternalImagePicker",
				"MoviePlayer",
				"SourceControl",
                "InternationalizationSettings",
				"SourceCodeAccess",
				"RHI",
                "MediaAssets",
                "HardwareTargeting",
				"SharedSettingsWidgets",
                "AIModule", 
                "MeshUtilities",
				"ConfigEditor",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
                "Engine",
                "Media",
				"Landscape",
				"LandscapeEditor",
				"PropertyEditor",
				"GameProjectGeneration",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"Layers",
				"PropertyEditor",
				"GameProjectGeneration",
			}
		);
	}
}
