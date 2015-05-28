// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CurveTableEditor : ModuleRules
{
	public CurveTableEditor(TargetInfo Target)
	{
		PublicIncludePathModuleNames.Add("LevelEditor");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"Engine", 
				"InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd"
			}
		);

		DynamicallyLoadedModuleNames.Add("WorkspaceMenuStructure");
	}
}
