// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LogVisualizer : ModuleRules
{
	public LogVisualizer(TargetInfo Target)
	{
        PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
				"MainFrame",
                "SequencerWidgets",
			}
		);

		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/Engine/Classes",
                "Editor/WorkspaceMenuStructure/Public"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Json",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"Engine",
				"UnrealEd",
                "SequencerWidgets",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
				"MainFrame",
			}
		);
	}
}
