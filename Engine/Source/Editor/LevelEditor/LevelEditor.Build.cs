// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelEditor : ModuleRules
{
	public LevelEditor(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"ClassViewer",
				"MainFrame",
                "PlacementMode",
				"ReferenceViewer",
				"SizeMap",
				"SlateReflector",
                "IntroTutorials",
                "AppFramework",
                "PortalServices"
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"UserFeedback",
				"IntroTutorials",
				"HeadMountedDisplay",
				"VREditor"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"LevelSequence",
				"Analytics",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"Engine",
				"MessageLog",
                "SourceControl",
                "SourceControlWindows",
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
                "Persona",
                "Kismet",
				"KismetWidgets",
				"Sequencer",
                "Foliage",
                "HierarchicalLODOutliner",
                "HierarchicalLODUtilities",
				"MaterialShaderQualitySettings",
                "PixelInspectorModule",
                "FunctionalTesting"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"ClassViewer",
				"DeviceManager",
				"SettingsEditor",
				"SessionFrontend",
				"SlateReflector",
				"AutomationWindow",
				"Layers",
                "WorldBrowser",
				"EditorWidgets",
				"AssetTools",
				"WorkspaceMenuStructure",
				"NewLevelDialog",
				"DeviceProfileEditor",
                "PlacementMode",
				"UserFeedback",
				"ReferenceViewer",
				"SizeMap",
                "IntroTutorials",
				"HeadMountedDisplay",
				"VREditor"
			}
		);
	}
}
