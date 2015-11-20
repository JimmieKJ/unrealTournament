// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SequencerWidgets : ModuleRules
{
	public SequencerWidgets(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Editor/SequencerWidgets/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "EditorStyle",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
				"UnrealEd"
			}
		);
	}
}
