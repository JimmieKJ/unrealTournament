// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
                "MovieScene",
                "VectorVM",
                "RHI",
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
