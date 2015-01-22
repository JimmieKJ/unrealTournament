// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelEditor : ModuleRules
{
	public LevelEditor(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"Kismet",
				"MainFrame",
                "PlacementMode",
				"ReferenceViewer",
                "IntroTutorials",
                "AppFramework"
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"UserFeedback",
				"IntroTutorials"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
                "InputCore",
				"Slate",
				"SlateCore",
				"SlateReflector",
                "EditorStyle",
				"Engine",
				"MessageLog",
				"NewsFeed",
                "SourceControl",
                "StatsViewer",
				"UnrealEd", 
				"RenderCore",
				"DeviceProfileServices",
				"ContentBrowser",
                "SceneOutliner",
                "ActorPickerMode",
                "RHI",
				"Projects",
				"TargetPlatform",
				"EngineSettings",
				"PropertyEditor",
				"WebBrowser",
                "Persona"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"PropertyEditor",
				"SceneOutliner",
				"ClassViewer",
				"DeviceManager",
				"SettingsEditor",
				"SessionFrontend",
				"AutomationWindow",
				"Layers",
				"Levels",
                "WorldBrowser",
				"TaskBrowser",
				"EditorWidgets",
				"AssetTools",
				"WorkspaceMenuStructure",
				"NewLevelDialog",
				"DeviceProfileEditor",
				"DeviceProfileServices",
                "PlacementMode",
				"UserFeedback",
				"ReferenceViewer",
                "IntroTutorials"
			}
		);
	}
}
