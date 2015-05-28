// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Niagara : ModuleRules
{
    public Niagara(TargetInfo Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
                "Engine",
				"RenderCore",
                "ShaderCore",
                "VectorVM",
                "RHI"
			}
        );


        PublicDependencyModuleNames.AddRange(
            new string[] {
                "RHI"
            }
        );


        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/Niagara/Private",
                "Runtime/Engine/Private",
                "Runtime/Engine/Classes/Engine"
            })
        ;
    }
}
