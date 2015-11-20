// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnitTestFrameworkEditor : ModuleRules
{
	public UnitTestFrameworkEditor(TargetInfo Target)
	{
        PrivateIncludePaths.Add("UnitTestFrameworkEditor/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "UnitTestFramework",
                "UnrealEd",
                "AssetRegistry",
                "SlateCore",
                "Kismet",
                "Slate",
                "EditorStyle",
                "Projects",
			}
		);
	}
}
 