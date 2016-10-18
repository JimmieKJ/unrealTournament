// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MeshUtilities : ModuleRules
{
	public MeshUtilities(TargetInfo Target)
	{

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "MaterialUtilities"
            }
        );

		PrivateDependencyModuleNames.AddRange(
			new string [] {
				"Core",
				"CoreUObject",
				"Engine",
				"RawMesh",
				"RenderCore", // For FPackedNormal
				"SlateCore",
				"MaterialUtilities", 
                "MeshBoneReduction",		
                "UnrealEd",
                "RHI",                
                "HierarchicalLODUtilities",
                "Landscape"
			}
		);

        AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTriStrip");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "ForsythTriOptimizer");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "MeshSimplifier");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "MikkTSpace");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTessLib");

		if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
		{
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX9");
		}

		if (UEBuildConfiguration.bCompileSimplygon == true)
		{
            AddEngineThirdPartyPrivateDynamicDependencies(Target, "SimplygonMeshReduction");
            
            if (UEBuildConfiguration.bCompileSimplygonSSF == true)
            {
                DynamicallyLoadedModuleNames.AddRange(
                    new string[] {
                    "SimplygonSwarm"
                }
                );
            }
		}
	}
}
