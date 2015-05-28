// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CodeEditor : ModuleRules
	{
		public CodeEditor(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/CodeEditor/Private",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"SlateCore",
					"Slate",
					"AssetTools",
					"UnrealEd",
					"EditorStyle",
					"PropertyEditor",
					"Kismet",  // for FWorkflowCentricApplication
					"InputCore",
					"DirectoryWatcher",
					"LevelEditor"
				}
				);
		}
	}
}