// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AlembicLibrary : ModuleRules
{
    public AlembicLibrary(TargetInfo Target)
	{
        PublicIncludePaths.Add("AlembicLibrary/Public");
        PrivateIncludePaths.Add("AlembicLibrary/Private"); 

        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
                "UnrealEd",
                "RawMesh",
                "GeometryCache",
                "AlembicLib",
                "MeshUtilities",
                "MaterialUtilities",
                "PropertyEditor",
                "SlateCore",
                "Slate",
                "EditorStyle",
                "Eigen",
                "RenderCore",
                "RHI"
			}
		);

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd"
            }
        );

    }
}
