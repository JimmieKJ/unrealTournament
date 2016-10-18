// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProceduralMeshComponentEditor : ModuleRules
	{
        public ProceduralMeshComponentEditor(TargetInfo Target)
		{
			PrivateIncludePaths.Add("ProceduralMeshComponentEditor/Private");
            PublicIncludePaths.Add("ProceduralMeshComponentEditor/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Slate",
                    "SlateCore",
                    "Engine",
                    "UnrealEd",
                    "PropertyEditor",
                    "RenderCore",
                    "ShaderCore",
                    "RHI",
                    "ProceduralMeshComponent",
                    "RawMesh",
                    "AssetTools",
                    "AssetRegistry"
                }
				);
		}
	}
}
