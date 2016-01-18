// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class IntroTutorials : ModuleRules
	{
		public IntroTutorials(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine", // @todo Mac: for some reason CoreUObject and Engine are needed to link in debug on Mac
                    "InputCore",
                    "Slate",
                    "EditorStyle",
                    "Documentation",
					"GraphEditor",
					"BlueprintGraph",
					"MessageLog"
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Editor/IntroTutorials/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "AppFramework",
                    "UnrealEd",
                    "Kismet",
                    "PlacementMode",
					"SlateCore",
					"Settings",
					"PropertyEditor",
					"DesktopPlatform",
					"AssetTools",
					"SourceCodeAccess",
					"ContentBrowser",
					"LevelEditor",
                    "AssetRegistry"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"MainFrame",
					"TargetPlatform",
					"TargetDeviceServices",
					"LauncherServices",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"MainFrame",
					"LauncherServices",
				}
			);
		}
	}
}