// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AnimGraph : ModuleRules
{
	public AnimGraph(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Editor/AnimGraph/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject", 
				"Engine", 
				"Slate",
				"AnimGraphRuntime",
				"BlueprintGraph",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"InputCore",
				"SlateCore",
				"UnrealEd",
                "GraphEditor",
				"PropertyEditor",
				"EditorStyle"
			}
		);

		CircularlyReferencedDependentModules.AddRange(
			new string[] {
                "UnrealEd",
                "GraphEditor",
            }
		); 
	}
}
