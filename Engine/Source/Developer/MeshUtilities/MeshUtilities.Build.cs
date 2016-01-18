// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
                "HierarchicalLODUtilities"
			}
		);

        AddThirdPartyPrivateStaticDependencies(Target, "nvTriStrip");
		AddThirdPartyPrivateStaticDependencies(Target, "ForsythTriOptimizer");
		AddThirdPartyPrivateStaticDependencies(Target, "MeshSimplifier");
		AddThirdPartyPrivateStaticDependencies(Target, "MikkTSpace");

		if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddThirdPartyPrivateStaticDependencies(Target, "DX9");
		}

		if (UEBuildConfiguration.bCompileSimplygon == true)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
                "SimplygonSwarm"
            }
			);

			AddThirdPartyPrivateDynamicDependencies(Target, "SimplygonMeshReduction");
		}
	}
}
