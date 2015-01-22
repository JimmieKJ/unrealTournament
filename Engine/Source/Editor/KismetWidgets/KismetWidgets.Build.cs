// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KismetWidgets : ModuleRules
{
	public KismetWidgets(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Editor/KismetWidgets/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"BlueprintGraph",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"ContentBrowser",
                "AssetTools",
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
			    "ContentBrowser",
                "AssetTools",
			}
		);
	}
}
