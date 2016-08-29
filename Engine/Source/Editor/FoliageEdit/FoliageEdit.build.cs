// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FoliageEdit : ModuleRules
{
	public FoliageEdit(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] 
			{
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

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ViewportInteraction",
				"VREditor"
			}
		);
    }
}
