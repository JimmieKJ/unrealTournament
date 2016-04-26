// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CollisionAnalyzer : ModuleRules
{
	public CollisionAnalyzer(TargetInfo Target)
	{
        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"DesktopPlatform",
				"MainFrame",
			}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"Engine",
				"WorkspaceMenuStructure",
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
