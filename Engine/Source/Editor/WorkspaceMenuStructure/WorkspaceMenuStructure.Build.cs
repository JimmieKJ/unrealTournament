// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorkspaceMenuStructure : ModuleRules
{
	public WorkspaceMenuStructure(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Editor/WorkspaceMenuStructure/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"EditorStyle",
				"SlateCore",
			}
		);
	}
}
