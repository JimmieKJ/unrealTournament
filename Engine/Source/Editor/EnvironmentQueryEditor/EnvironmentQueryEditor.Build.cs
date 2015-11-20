// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EnvironmentQueryEditor : ModuleRules
{
	public EnvironmentQueryEditor(TargetInfo Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
				"Editor/GraphEditor/Private",
				"Editor/AIGraph/Private",
				"Editor/EnvironmentQueryEditor/Private",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"AssetTools",
                "PropertyEditor"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
                "InputCore",
				"Engine", 
                "RenderCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd", 
				"MessageLog", 
				"GraphEditor",
				"KismetWidgets",
                "PropertyEditor",
				"AnimGraph",
				"BlueprintGraph",
                "AIGraph",
                "AIModule",
			}
		);

		PublicIncludePathModuleNames.Add("LevelEditor");

        DynamicallyLoadedModuleNames.AddRange(
            new string[] { 
                "WorkspaceMenuStructure",
            }
		);
	}
}
