// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class StaticMeshEditor : ModuleRules
{
	public StaticMeshEditor(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"Kismet",
				"EditorWidgets",
				"MeshUtilities",				
                "PropertyEditor"
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
				"ShaderCore",
				"RenderCore",
				"RHI",
				"UnrealEd",
				"TargetPlatform",
				"RawMesh",
                "PropertyEditor",
				"MeshUtilities",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"SceneOutliner",
				"ClassViewer",
				"ContentBrowser",
				"WorkspaceMenuStructure",
			}
		);

		SetupModulePhysXAPEXSupport(Target);
	}
}
