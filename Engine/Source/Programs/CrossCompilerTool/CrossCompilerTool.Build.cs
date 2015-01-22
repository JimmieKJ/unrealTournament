// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrossCompilerTool : ModuleRules
{
	public CrossCompilerTool(TargetInfo Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Developer/iOS/MetalShaderFormat/Private");		// For Metal includes
		PrivateIncludePaths.Add("Developer/ShaderFormatOpenGL/Private");		// For GLSL includes
		PrivateIncludePaths.Add("Developer/ShaderCompilerCommon/Private");		// For Lexer includes

		PrivateDependencyModuleNames.AddRange(new string []
			{
				"Core",
				"Projects",
				"ShaderCompilerCommon",
				"MetalShaderFormat",
				"ShaderFormatOpenGL",
				"ShaderPreprocessor",

				//@todo-rco: Remove me!
				"ShaderCore",
			});

		AddThirdPartyPrivateStaticDependencies(Target,
			"HLSLCC"
		);
	}
}
