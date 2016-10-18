// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
		    AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");

		if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
		{
		    AddEngineThirdPartyPrivateStaticDependencies(Target, "ANGLE"); 
		}

		if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.HTML5)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
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

		if(Target.Platform != UnrealTargetPlatform.Win32 && Target.Platform != UnrealTargetPlatform.Win64 && 
			Target.Platform != UnrealTargetPlatform.Mac && Target.Platform != UnrealTargetPlatform.IOS && 
			Target.Platform != UnrealTargetPlatform.Android && Target.Platform != UnrealTargetPlatform.HTML5 && 
			Target.Platform != UnrealTargetPlatform.Linux)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
