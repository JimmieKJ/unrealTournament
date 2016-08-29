// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SequencerWidgets : ModuleRules
{
	public SequencerWidgets(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Editor/SequencerWidgets/Private");

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "MovieScene",
            }
           );

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "MovieScene",
            }
        );
        
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
                "MovieScene",
                "UnrealEd"
			}
		);
	}
}
