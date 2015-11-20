// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FoliageEdit : ModuleRules
{
	public FoliageEdit(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"UnrealEd",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"RenderCore",
				"LevelEditor",
				"Landscape",
                "PropertyEditor",
                "DetailCustomizations",
                "AssetTools",
                "Foliage",
			}
		);
	}
}
