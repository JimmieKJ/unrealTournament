// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryMode : ModuleRules
{
	public GeometryMode(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"RenderCore",
				"LevelEditor"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"PropertyEditor",
                "BspMode"
			}
        );
	}
}
