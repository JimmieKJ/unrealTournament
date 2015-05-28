// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MergeActors : ModuleRules
{
	public MergeActors(TargetInfo Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
				"Editor/MergeActors/Private",
				"Editor/MergeActors/Private/MeshMergingTool",
				"Editor/MergeActors/Private/MeshProxyTool"
			}
        );

        PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "ContentBrowser",
                "Documentation",
                "LevelEditor",
                "MeshUtilities",
                "PropertyEditor",
                "RawMesh",
                "WorkspaceMenuStructure"
			}
		);

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
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "ContentBrowser",
                "Documentation",
                "LevelEditor",
                "MeshUtilities",
                "PropertyEditor"
			}
		);
	}
}
