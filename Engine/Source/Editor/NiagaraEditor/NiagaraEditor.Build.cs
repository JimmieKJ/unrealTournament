// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraEditor : ModuleRules
{
	public NiagaraEditor(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(new string[] {
			"Editor/NiagaraEditor/Private",
		});

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
				"VectorVM",
                "Niagara",
                "MovieScene",
				"Sequencer",
				"PropertyEditor",
				"GraphEditor"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Engine",
				"Messaging",
				"LevelEditor",
				"AssetTools"
			}
		);

		PublicDependencyModuleNames.AddRange(
            new string[] {
				"Engine",
                "Niagara",
                "UnrealEd",
            }
        );

        PublicIncludePathModuleNames.AddRange(
            new string[] {
				"Engine",
				"Niagara"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "WorkspaceMenuStructure",
                }
            );
	}
}
