// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenGLDrv : ModuleRules
{
	public OpenGLDrv(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/OpenGLDrv/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"Engine", 
				"RHI", 
				"RenderCore", 
				"ShaderCore",
				"UtilityShaders",
			}
			);

		PrivateIncludePathModuleNames.Add("ImageWrapper");
		DynamicallyLoadedModuleNames.Add("ImageWrapper");

        if (Target.Platform != UnrealTargetPlatform.HTML5)
		    AddThirdPartyPrivateStaticDependencies(Target, "OpenGL");

		if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
		{
		    AddThirdPartyPrivateStaticDependencies(Target, "ANGLE"); 
		}

		if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.HTML5)
		{
			AddThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"TaskGraph"
                }
			);
		}
	}
}
