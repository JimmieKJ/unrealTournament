// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SimplygonMeshReduction : ModuleRules
{
	public SimplygonMeshReduction(TargetInfo Target)
	{
		BinariesSubFolder = "NotForLicensees";

        PublicIncludePaths.Add("Developer/SimplygonMeshReduction/Public");

        PrivateDependencyModuleNames.AddRange(
            new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
                "RawMesh",
                "MaterialUtilities",
                "MeshBoneReduction",
                "RHI"
			}
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] { 
                "MeshUtilities"                
            }
        );
        		
		AddEngineThirdPartyPrivateStaticDependencies(Target, "Simplygon");		

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
