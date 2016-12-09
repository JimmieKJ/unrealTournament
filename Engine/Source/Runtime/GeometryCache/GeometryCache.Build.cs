// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

        PublicIncludePathModuleNames.Add("TargetPlatform");

        if (UEBuildConfiguration.bBuildEditor)
        {
            PublicIncludePathModuleNames.Add("GeometryCacheEd");
            DynamicallyLoadedModuleNames.Add("GeometryCacheEd");
        }        
	}
}
