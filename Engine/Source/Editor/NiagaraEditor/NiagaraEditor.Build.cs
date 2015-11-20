// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraEditor : ModuleRules
{
	public NiagaraEditor(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine", 
				"Core", 
				"CoreUObject", 
                "InputCore",
				"RenderCore",
				"Slate", 
				"SlateCore",
				"Kismet",
                "EditorStyle",
				"UnrealEd", 
				"GraphEditor", 
				"VectorVM",
                "Niagara",
                "MovieScene",
				"Sequencer",
				"PropertyEditor",
			}
		);

        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Engine",
                "Niagara",
				"PropertyEditor",
            }
        );

        PublicIncludePathModuleNames.AddRange(
            new string[] {
				"Engine", 
				"Messaging", 
				"GraphEditor", 
				"LevelEditor"}
                );

		DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "WorkspaceMenuStructure",
                }
            );
	}
}
