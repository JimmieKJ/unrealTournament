// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FbxAutomationBuilderModule : ModuleRules
{
    public FbxAutomationBuilderModule(TargetInfo Target)
	{
        PublicDependencyModuleNames.AddRange
        (
            new string[] {
				"Core",
                "InputCore",
                "CoreUObject",
				"Slate",
                "EditorStyle",
                "Engine",
                "UnrealEd",
                "PropertyEditor",
			}
        );
        
        PrivateDependencyModuleNames.AddRange(
             new string[] {
					"Engine",
                    "UnrealEd"
				}
         );

        if (UEBuildConfiguration.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "UnrealEd",
    				"SlateCore",
    				"Slate",
                }
            );

            CircularlyReferencedDependentModules.AddRange(
                new string[] {
                    "UnrealEd",
                    "LevelEditor"
                }
            );
        }
	}
}
