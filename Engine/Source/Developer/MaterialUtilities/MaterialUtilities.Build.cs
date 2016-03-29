// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MaterialUtilities : ModuleRules
{
	public MaterialUtilities(TargetInfo Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string [] {
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
                "RHI",
                "Landscape",
                "UnrealEd",
                "ShaderCore"
			}
		);

        PublicDependencyModuleNames.AddRange(
			new string [] {
				 "RawMesh",            
			}
		);      

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "Landscape"
			}
        );

        CircularlyReferencedDependentModules.AddRange(
            new string[] {
                "Landscape"
            }
        );
	}
}
