// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AlembicImporter : ModuleRules
{
    public AlembicImporter(TargetInfo Target)
	{
        PrivateIncludePaths.Add("AlembicImporter/Private");
        PublicIncludePaths.Add("AlembicImporter/Public");
        PublicIncludePaths.Add("AlembicImporter/Classes");

        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
                "MainFrame",
                "PropertyEditor",
                "AlembicLibrary",
                "GeometryCache",
                "RenderCore",
                "RHI"
            }
		);
	}
}
