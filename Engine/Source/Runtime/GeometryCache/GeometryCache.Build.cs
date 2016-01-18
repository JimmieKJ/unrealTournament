// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryCache : ModuleRules
{
	public GeometryCache(TargetInfo Target)
	{
        PublicIncludePaths.Add("Runtime/GeometryCache/Public");
        PublicIncludePaths.Add("Runtime/GeometryCache/Classes");
        PrivateIncludePaths.Add("Runtime/GeometryCache/Private");
        
        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "InputCore",
                "RenderCore",
                "ShaderCore",
                "RHI"
			}
		);

        PublicIncludePathModuleNames.Add("GeometryCacheEd");

        if (UEBuildConfiguration.bBuildEditor)
        {
            DynamicallyLoadedModuleNames.Add("GeometryCacheEd");
        }
	}
}
