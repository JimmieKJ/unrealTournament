// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BlueprintGraph : ModuleRules
{
	public BlueprintGraph(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(
            new string[] {
                "Editor/BlueprintGraph/Private",
                "Editor/KismetCompiler/Public",
            }
		);

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject", 
				"Engine",
                "InputCore",
				"Slate",
                "EditorStyle",
                "AIModule",
			}
		);

		PrivateDependencyModuleNames.AddRange( 
			new string[] {
				"EditorStyle",
                "KismetCompiler",
				"UnrealEd",
                "GraphEditor",
				"SlateCore",
                "Kismet",
                "PropertyEditor",
			}
		);

		CircularlyReferencedDependentModules.AddRange(
            new string[] {
                "KismetCompiler",
                "UnrealEd",
                "GraphEditor",
                "Kismet",
            }
		); 
	}
}
