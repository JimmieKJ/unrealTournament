// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FunctionalTestingEditor : ModuleRules
{
    public FunctionalTestingEditor(TargetInfo Target)
	{
        PublicDependencyModuleNames.AddRange
        (
            new string[] {
				"Core",
                "InputCore",
                "CoreUObject",
                "SlateCore",
                "Slate",
                "EditorStyle",
                "Engine",
                "AssetRegistry"
			}
        );
        
        PrivateDependencyModuleNames.AddRange(
             new string[] {
                "Engine",
                "UnrealEd",
                "LevelEditor",
                "SessionFrontend",
                "FunctionalTesting",
                "PlacementMode",
                "WorkspaceMenuStructure",
                "ScreenShotComparisonTools"
            }
         );
	}
}
