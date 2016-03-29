// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShaderFormatOpenGL : ModuleRules
{
	public ShaderFormatOpenGL(TargetInfo Target)
	{

		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PrivateIncludePathModuleNames.Add("OpenGLDrv"); 

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ShaderCore",
				"ShaderCompilerCommon",
				"ShaderPreprocessor"
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"OpenGL",
			"HLSLCC"
			);

        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
        }
	}
}
