// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
                "MovieScene",
				"Slate",
				"SlateCore",
				"UnrealEd"
			}
		);
	}
}
