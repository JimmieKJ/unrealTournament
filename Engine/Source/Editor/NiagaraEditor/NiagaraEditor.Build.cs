// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraEditor : ModuleRules
{
	public NiagaraEditor(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
                "RHI",
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
                "UnrealEd",
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
