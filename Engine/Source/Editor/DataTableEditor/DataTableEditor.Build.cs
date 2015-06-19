// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DataTableEditor : ModuleRules
{
	public DataTableEditor(TargetInfo Target)
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
                "PropertyEditor",
				"UnrealEd",
				"Json"
			}
			);

		DynamicallyLoadedModuleNames.Add("WorkspaceMenuStructure");
	}
}